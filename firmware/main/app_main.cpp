#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "model/app_state.hpp"
#include "ui/ui_app.hpp"

namespace {

constexpr char kTag[] = "codex_island";
codex_island::AppStateStore g_state;
codex_island::ui::UiApp g_ui;

void ui_tick_task(void *) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (bsp_display_lock(100) == ESP_OK) {
            g_ui.tick(esp_timer_get_time() / 1'000'000);
            bsp_display_unlock();
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
    xTaskCreate(ui_tick_task, "ui_tick", 4096, nullptr, 5, nullptr);
}
