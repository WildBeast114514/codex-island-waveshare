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
#include "protocol/protocol.hpp"
#include "transport/ble_nus.hpp"
#include "ui/ui_app.hpp"

namespace {

constexpr char kTag[] = "codex_island";
codex_island::AppStateStore g_state;
codex_island::ui::UiApp g_ui;
codex_island::protocol::ProtocolProcessor g_protocol;
codex_island::transport::ble_nus::Line g_protocol_line{};
std::atomic<bool> g_ui_dirty{false};

void ui_tick_task(void *) {
    int64_t last_tick = -1;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
        const int64_t now = esp_timer_get_time() / 1'000'000;
        const bool one_second_elapsed = now != last_tick;
        const bool refresh_requested = g_ui_dirty.exchange(false);
        if (!one_second_elapsed && !refresh_requested) {
            continue;
        }
        if (bsp_display_lock(100) == ESP_OK) {
            if (one_second_elapsed) {
                g_ui.tick(now);
                last_tick = now;
            } else {
                g_ui.refresh(now);
            }
            bsp_display_unlock();
        }
    }
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
                set_link_connected(true);
            } else if (event == LinkEvent::kDisconnected) {
                set_link_connected(false);
            } else if (event == LinkEvent::kSubscribed) {
                const bool sent = codex_island::transport::ble_nus::notify_json_line(
                    "{\"v\":1,\"k\":\"hello\",\"fw\":\"0.1.0\"}");
                ESP_LOGI(kTag, "hello notification %s", sent ? "sent" : "failed");
            }
        }

        if (!codex_island::transport::ble_nus::poll_line(
                g_protocol_line, pdMS_TO_TICKS(100))) {
            continue;
        }
        const int64_t now = esp_timer_get_time() / 1'000'000;
        const codex_island::protocol::ProcessResult result =
            g_protocol.process_line(g_protocol_line.data, now, g_state);
        if (result == codex_island::protocol::ProcessResult::kInvalid) {
            ESP_LOGE(kTag, "invalid BLE protocol line; previous state retained");
        } else {
            ESP_LOGI(kTag, "BLE protocol: %s; protocol stack free=%u",
                     codex_island::protocol::result_name(result),
                     static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
            g_ui_dirty.store(true);
        }
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

    g_state.replace(codex_island::make_static_mock_state());
    lv_display_t *display = bsp_display_start();
    ESP_ERROR_CHECK(display == nullptr ? ESP_FAIL : ESP_OK);
    ESP_ERROR_CHECK(bsp_display_brightness_set(35));

    ESP_ERROR_CHECK(bsp_display_lock(static_cast<uint32_t>(-1)));
    g_ui.begin(&g_state);
    const int width = lv_display_get_horizontal_resolution(display);
    const int height = lv_display_get_vertical_resolution(display);
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
        xTaskCreate(ui_tick_task, "ui_tick", 4096, nullptr, 5, nullptr);
    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    task_result =
        xTaskCreate(protocol_task, "protocol", 12288, nullptr, 6, nullptr);
    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
