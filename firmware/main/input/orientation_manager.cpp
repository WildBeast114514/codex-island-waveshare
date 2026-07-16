#include "input/orientation_manager.hpp"

#include <cmath>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace codex_island::input {

namespace {

constexpr char kTag[] = "orientation";
constexpr uint8_t kResetRegister = 0x60;
constexpr uint8_t kResetCommand = 0xB0;
constexpr uint8_t kResetResultRegister = 0x4D;
constexpr uint8_t kResetComplete = 0x80;

esp_err_t reset_sensor(qmi8658_dev_t &sensor) {
    esp_err_t result =
        qmi8658_write_register(&sensor, kResetRegister, kResetCommand);
    if (result != ESP_OK) {
        return result;
    }

    for (int attempt = 0; attempt < 50; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(10));
        uint8_t reset_result = 0;
        result = qmi8658_read_register(
            &sensor, kResetResultRegister, &reset_result, 1);
        if (result == ESP_OK && reset_result == kResetComplete) {
            return ESP_OK;
        }
    }
    return ESP_ERR_TIMEOUT;
}

}  // namespace

esp_err_t OrientationManager::begin() {
    esp_err_t result =
        qmi8658_init(&sensor_, bsp_i2c_get_handle(), QMI8658_ADDRESS_HIGH);
    if (result != ESP_OK) {
        return result;
    }

    // The component's reset helper writes CTRL1 instead of the QMI8658 reset
    // register. Perform the reset sequence used by Waveshare's SensorLib so
    // warm boots and partially configured sensors recover reliably.
    result = reset_sensor(sensor_);
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "QMI8658 reset failed: %s", esp_err_to_name(result));
        return result;
    }

    // Address auto-increment, little-endian data. The component parser expects
    // little-endian samples, so CTRL1 must not select the big-endian format.
    result = qmi8658_write_register(&sensor_, QMI8658_CTRL1, 0x40);
    if (result != ESP_OK) {
        return result;
    }
    result = qmi8658_enable_sensors(&sensor_, QMI8658_DISABLE_ALL);
    if (result != ESP_OK) {
        return result;
    }
    result = qmi8658_set_accel_range(&sensor_, QMI8658_ACCEL_RANGE_4G);
    if (result != ESP_OK) {
        return result;
    }
    result = qmi8658_set_accel_odr(&sensor_, QMI8658_ACCEL_ODR_125HZ);
    if (result != ESP_OK) {
        return result;
    }
    result = qmi8658_write_register(&sensor_, QMI8658_CTRL5, 0x03);
    if (result != ESP_OK) {
        return result;
    }
    result = qmi8658_enable_sensors(&sensor_, QMI8658_ENABLE_ACCEL);
    if (result != ESP_OK) {
        return result;
    }
    qmi8658_set_accel_unit_mg(&sensor_, true);
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t registers[4]{};
    const uint8_t addresses[] = {
        QMI8658_CTRL1, QMI8658_CTRL2, QMI8658_CTRL5, QMI8658_CTRL7};
    for (size_t index = 0; index < sizeof(addresses); ++index) {
        result = qmi8658_read_register(
            &sensor_, addresses[index], &registers[index], 1);
        if (result != ESP_OK) {
            return result;
        }
    }
    ready_ = true;
    ESP_LOGI(kTag,
             "QMI8658 gravity auto-rotation ready "
             "(CTRL1=%02x CTRL2=%02x CTRL5=%02x CTRL7=%02x)",
             registers[0], registers[1], registers[2], registers[3]);
    return ESP_OK;
}

bool OrientationManager::poll(DisplayOrientation &orientation) {
    if (!ready_) {
        return false;
    }

    qmi8658_data_t data{};
    const esp_err_t result = qmi8658_read_sensor_data(&sensor_, &data);
    if (result != ESP_OK) {
        ESP_LOGW(kTag, "accelerometer read failed: %s",
                 esp_err_to_name(result));
        return false;
    }
    const auto changed = filter_.update(
        static_cast<int>(std::lround(data.accelX)),
        static_cast<int>(std::lround(data.accelY)),
        static_cast<int>(std::lround(data.accelZ)));
    if (!changed.has_value()) {
        return false;
    }

    orientation = *changed;
    ESP_LOGI(kTag, "gravity=(%.0f,%.0f,%.0f)mg -> %s degrees",
             static_cast<double>(data.accelX),
             static_cast<double>(data.accelY),
             static_cast<double>(data.accelZ),
             orientation_name(orientation));
    return true;
}

}  // namespace codex_island::input
