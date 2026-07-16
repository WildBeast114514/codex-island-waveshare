#include <algorithm>
#include <atomic>
#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "model/app_state.hpp"
#include "input/input_manager.hpp"
#include "input/orientation_manager.hpp"
#include "power/power_manager.hpp"
#include "protocol/protocol.hpp"
#include "storage/state_cache.hpp"
#include "transport/ble_nus.hpp"
#include "transport/link_watchdog.hpp"
#include "ui/ui_app.hpp"

namespace {

constexpr char kTag[] = "codex_island";
codex_island::AppStateStore g_state;
codex_island::ui::UiApp g_ui;
codex_island::protocol::ProtocolProcessor g_protocol;
codex_island::transport::ble_nus::Line g_protocol_line{};
codex_island::power::PowerManager g_power;
codex_island::input::InputManager g_input;
codex_island::input::OrientationManager g_orientation;
codex_island::storage::StateCache g_cache;
std::atomic<bool> g_ui_dirty{false};
std::atomic<bool> g_next_page{false};
std::atomic<bool> g_toggle_screen{false};
std::atomic<bool> g_wake_screen{false};
std::atomic<bool> g_apply_shift{false};
std::atomic<int8_t> g_shift_x{0};
std::atomic<int8_t> g_shift_y{0};
std::atomic<int64_t> g_last_activity{0};
std::atomic<bool> g_screen_on{true};
std::atomic<uint8_t> g_configured_brightness{35};
std::atomic<uint8_t> g_actual_brightness{35};
std::atomic<int16_t> g_brightness_request{-1};
std::atomic<int64_t> g_last_ble_activity{0};
std::atomic<int8_t> g_rotation_request{-1};
lv_display_t *g_display = nullptr;

lv_display_rotation_t touch_rotation(
    codex_island::input::DisplayOrientation orientation) {
    return static_cast<lv_display_rotation_t>(
        codex_island::input::inverse_orientation(orientation));
}

bsp_display_rotation_t bsp_rotation(
    codex_island::input::DisplayOrientation orientation) {
    return static_cast<bsp_display_rotation_t>(orientation);
}

void ui_tick_task(void *) {
    int64_t last_tick = -1;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
        const int64_t now = esp_timer_get_time() / 1'000'000;
        const bool one_second_elapsed = now != last_tick;
        const bool refresh_requested = g_ui_dirty.exchange(false);
        const bool next_page = g_next_page.exchange(false);
        const bool apply_shift = g_apply_shift.exchange(false);
        const int8_t requested_rotation = g_rotation_request.exchange(-1);
        if (g_ui.take_user_activity()) {
            g_last_activity.store(now);
            g_wake_screen.store(true);
        }
        uint8_t requested_brightness = 0;
        if (g_ui.take_brightness_request(requested_brightness)) {
            g_brightness_request.store(requested_brightness);
            g_last_activity.store(now);
            g_wake_screen.store(true);
        }
        if (!one_second_elapsed && !refresh_requested && !next_page &&
            !apply_shift && requested_rotation < 0) {
            continue;
        }
        ESP_ERROR_CHECK(bsp_display_lock(static_cast<uint32_t>(-1)));
        if (requested_rotation >= 0) {
            lv_indev_t *touch = bsp_display_get_input_dev();
            if (touch != nullptr &&
                lv_indev_get_state(touch) == LV_INDEV_STATE_PRESSED) {
                g_rotation_request.store(requested_rotation);
            } else {
                const auto orientation =
                    static_cast<codex_island::input::DisplayOrientation>(
                        requested_rotation);
                const esp_err_t result =
                    bsp_display_rotation_set(bsp_rotation(orientation));
                if (result == ESP_OK) {
                    lv_display_set_rotation(
                        g_display, touch_rotation(orientation));
                    lv_obj_invalidate(lv_screen_active());
                    ESP_LOGI(kTag, "display auto-rotated to %s degrees",
                             codex_island::input::orientation_name(orientation));
                } else {
                    ESP_LOGE(kTag, "display rotation failed: %s",
                             esp_err_to_name(result));
                }
            }
        }
        if (next_page) {
            g_ui.next_page();
        }
        if (apply_shift) {
            g_ui.set_pixel_offset(g_shift_x.load(), g_shift_y.load());
        }
        if (one_second_elapsed) {
            g_ui.tick(now);
            last_tick = now;
        } else {
            g_ui.refresh(now);
        }
        bsp_display_unlock();
    }
}

void orientation_task(void *) {
    const esp_err_t result = g_orientation.begin();
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "QMI8658 unavailable; auto-rotation disabled: %s",
                 esp_err_to_name(result));
        vTaskDelete(nullptr);
        return;
    }

    while (true) {
        codex_island::input::DisplayOrientation orientation{};
        if (g_orientation.poll(orientation)) {
            g_rotation_request.store(static_cast<int8_t>(orientation));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void set_display_brightness(uint8_t brightness) {
    if (g_actual_brightness.load() == brightness) {
        return;
    }
    if (bsp_display_lock(static_cast<uint32_t>(-1)) == ESP_OK) {
        if (bsp_display_brightness_set(brightness) == ESP_OK) {
            g_actual_brightness.store(brightness);
        }
        bsp_display_unlock();
    }
}

void set_screen_on(bool enabled) {
    g_screen_on.store(enabled);
    set_display_brightness(enabled ? g_configured_brightness.load() : 0);
    ESP_LOGI(kTag, "display %s", enabled ? "on" : "off");
}

void set_link_connected(bool connected) {
    g_state.update([connected](codex_island::AppState &state) {
        state.link.ble_connected = connected;
    });
    g_ui_dirty.store(true);
}

void protocol_task(void *) {
    using codex_island::transport::ble_nus::LinkEvent;
    while (true) {
        LinkEvent event{};
        while (codex_island::transport::ble_nus::poll_link_event(event)) {
            if (event == LinkEvent::kConnected) {
                // Sequence numbers are scoped to one BLE central session. A
                // restarted macOS Bridge intentionally starts from one again.
                g_protocol.begin_session();
                g_last_ble_activity.store(esp_timer_get_time() / 1'000'000);
                set_link_connected(true);
            } else if (event == LinkEvent::kDisconnected) {
                set_link_connected(false);
            } else if (event == LinkEvent::kSubscribed) {
                const bool sent = codex_island::transport::ble_nus::notify_json_line(
                "{\"v\":1,\"k\":\"hello\",\"fw\":\"0.3.0\"}");
                ESP_LOGI(kTag, "hello notification %s", sent ? "sent" : "failed");
            }
        }

        if (!codex_island::transport::ble_nus::poll_line(
                g_protocol_line, pdMS_TO_TICKS(100))) {
            continue;
        }
        const int64_t now = esp_timer_get_time() / 1'000'000;
        g_last_ble_activity.store(now);
        const codex_island::protocol::ProcessResult result =
            g_protocol.process_line(g_protocol_line.data, now, g_state);
        if (result == codex_island::protocol::ProcessResult::kInvalid) {
            ESP_LOGE(kTag, "invalid BLE protocol line; previous state retained");
        } else {
            ESP_LOGI(kTag, "BLE protocol: %s; protocol stack free=%u",
                     codex_island::protocol::result_name(result),
                     static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
            if (result != codex_island::protocol::ProcessResult::kHeartbeat) {
                g_ui_dirty.store(true);
            }
        }
    }
}

void power_input_task(void *) {
    uint32_t last_power_read_ms = 0;
    uint32_t last_cache_check_ms = 0;
    int64_t last_auto_page = 0;
    int64_t last_pixel_shift = 0;
    uint8_t shift_step = 0;
    while (true) {
        const int64_t now_seconds = esp_timer_get_time() / 1'000'000;
        const uint32_t now_ms =
            static_cast<uint32_t>(esp_timer_get_time() / 1'000);
        const codex_island::input::ButtonEvents button = g_input.poll(now_ms);
        if (button.short_press) {
            g_last_activity.store(now_seconds);
            g_wake_screen.store(true);
            g_next_page.store(true);
        }
        if (button.double_press) {
            g_last_activity.store(now_seconds);
            g_wake_screen.store(true);
            const bool sent = codex_island::transport::ble_nus::notify_json_line(
                "{\"v\":1,\"k\":\"refresh\",\"what\":\"all\"}");
            ESP_LOGI(kTag, "manual refresh request %s",
                     sent ? "sent" : "not connected");
        }

        const codex_island::power::PowerKeyEvent power_key = g_power.poll_key();
        if (power_key == codex_island::power::PowerKeyEvent::kShort) {
            g_last_activity.store(now_seconds);
            g_toggle_screen.store(true);
        } else if (power_key == codex_island::power::PowerKeyEvent::kLong) {
            ESP_LOGI(kTag, "AXP2101 long-press observed; PMIC behavior preserved");
        }

        const int16_t requested_brightness = g_brightness_request.exchange(-1);
        if (requested_brightness >= 0) {
            const uint8_t brightness = static_cast<uint8_t>(
                std::clamp<int>(requested_brightness, 5, 100));
            g_configured_brightness.store(brightness);
            if (!g_screen_on.load()) {
                set_screen_on(true);
            } else {
                set_display_brightness(brightness);
            }
            ESP_LOGI(kTag, "brightness set to %u%%",
                     static_cast<unsigned>(brightness));
        }

        if (codex_island::transport::link_needs_recovery(
                codex_island::transport::ble_nus::is_connected(),
                g_last_ble_activity.load(), now_seconds) &&
            codex_island::transport::ble_nus::terminate_connection()) {
            g_last_ble_activity.store(now_seconds);
            ESP_LOGW(kTag,
                     "BLE application traffic timed out; recovering advertisement");
        }

        if (now_ms - last_power_read_ms >= 5'000) {
            codex_island::PowerState power{};
            if (g_power.read_state(power) == ESP_OK) {
                g_state.update([&power](codex_island::AppState &state) {
                    state.power = power;
                });
                g_ui_dirty.store(true);
                ESP_LOGI(kTag, "power: battery=%s%u%% charging=%s usb=%s",
                         power.battery_present ? "" : "absent/",
                         power.battery_percent, power.charging ? "yes" : "no",
                         power.usb_present ? "yes" : "no");
            }
            last_power_read_ms = now_ms;
        }

        if (g_toggle_screen.exchange(false)) {
            set_screen_on(!g_screen_on.load());
        }
        if (g_wake_screen.exchange(false) && !g_screen_on.load()) {
            set_screen_on(true);
        }

        const codex_island::AppState snapshot = g_state.snapshot();
        const int64_t idle_seconds =
            std::max<int64_t>(0, now_seconds - g_last_activity.load());
        if (snapshot.power.usb_present) {
            if (g_screen_on.load()) {
                set_display_brightness(g_configured_brightness.load());
            }
            if (g_screen_on.load() && idle_seconds >= 60 &&
                now_seconds - last_auto_page >= 15) {
                g_next_page.store(true);
                last_auto_page = now_seconds;
            }
            if (now_seconds - last_pixel_shift >= 300) {
                constexpr int8_t offsets[][2] = {
                    {-1, 0}, {1, 1}, {0, -1}, {-2, 1}, {2, -1}, {0, 0}};
                const auto &offset = offsets[shift_step % 6];
                g_shift_x.store(offset[0]);
                g_shift_y.store(offset[1]);
                g_apply_shift.store(true);
                ++shift_step;
                last_pixel_shift = now_seconds;
            }
        } else if (idle_seconds >= 120) {
            if (g_screen_on.load()) {
                set_screen_on(false);
            }
        } else if (idle_seconds >= 30 && g_screen_on.load()) {
            set_display_brightness(10);
        } else if (g_screen_on.load()) {
            set_display_brightness(g_configured_brightness.load());
        }

        if (now_ms - last_cache_check_ms >= 10'000) {
            const esp_err_t saved = g_cache.save_if_changed(
                g_state.snapshot(), g_ui.current_page(),
                g_configured_brightness.load());
            if (saved != ESP_OK) {
                ESP_LOGW(kTag, "NVS cache update failed: %s",
                         esp_err_to_name(saved));
            }
            ESP_LOGI(kTag, "power/input stack free=%u",
                     static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
            last_cache_check_ms = now_ms;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

}  // namespace

extern "C" void app_main(void) {
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);

    ESP_ERROR_CHECK(g_cache.begin());
    static codex_island::AppState initial_state{};
    uint8_t initial_page = 0;
    uint8_t initial_brightness = 35;
    const bool cache_loaded =
        g_cache.load(initial_state, initial_page, initial_brightness);
    g_state.replace(initial_state);
    g_configured_brightness.store(initial_brightness);
    g_actual_brightness.store(initial_brightness);
    ESP_LOGI(kTag, "NVS cache %s", cache_loaded ? "loaded" : "empty");

    g_display = bsp_display_start();
    ESP_ERROR_CHECK(g_display == nullptr ? ESP_FAIL : ESP_OK);
    ESP_ERROR_CHECK(bsp_display_brightness_set(initial_brightness));

    const esp_err_t power_result = g_power.begin();
    if (power_result == ESP_OK) {
        codex_island::PowerState power{};
        if (g_power.read_state(power) == ESP_OK) {
            g_state.update([&power](codex_island::AppState &state) {
                state.power = power;
            });
        }
    } else {
        ESP_LOGE(kTag, "AXP2101 unavailable: %s", esp_err_to_name(power_result));
    }
    ESP_ERROR_CHECK(g_input.begin());
    g_last_activity.store(esp_timer_get_time() / 1'000'000);

    ESP_ERROR_CHECK(bsp_display_lock(static_cast<uint32_t>(-1)));
    g_ui.begin(&g_state, initial_page, initial_brightness);
    const int width = lv_display_get_horizontal_resolution(g_display);
    const int height = lv_display_get_vertical_resolution(g_display);
    bsp_display_unlock();

    ESP_LOGI(kTag, "native LVGL display: %dx%d", width, height);
    ESP_LOGI(kTag, "free PSRAM after UI: %u bytes",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    ESP_LOGI(kTag, "free internal/DMA after UI: %u/%u bytes (largest DMA %u)",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_DMA)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));

    uint8_t bluetooth_mac[6]{};
    ESP_ERROR_CHECK(esp_read_mac(bluetooth_mac, ESP_MAC_BT));
    char device_name[32]{};
    std::snprintf(device_name, sizeof(device_name), "Codex Island-%02X%02X",
                  bluetooth_mac[4], bluetooth_mac[5]);
    ESP_ERROR_CHECK(codex_island::transport::ble_nus::start(device_name));
    ESP_LOGI(kTag, "BLE device name: %s", device_name);
    ESP_LOGI(kTag, "free internal/DMA after BLE: %u/%u bytes (largest DMA %u)",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_DMA)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));

    BaseType_t task_result =
        xTaskCreate(ui_tick_task, "ui_tick", 12288, nullptr, 5, nullptr);
    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    task_result =
        xTaskCreate(protocol_task, "protocol", 12288, nullptr, 6, nullptr);
    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    task_result =
        xTaskCreate(power_input_task, "power_input", 12288, nullptr, 4, nullptr);
    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    task_result =
        xTaskCreate(orientation_task, "orientation", 4096, nullptr, 3, nullptr);
    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
