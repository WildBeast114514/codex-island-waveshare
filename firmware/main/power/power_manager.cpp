#include "power/power_manager.hpp"

#include <algorithm>

#include "bsp/esp-bsp.h"
#include "esp_log.h"

namespace codex_island::power {
namespace {

constexpr char kTag[] = "power";
constexpr uint8_t kAddress = 0x34;
constexpr uint8_t kExpectedChipId = 0x4A;
constexpr uint8_t kStatus1 = 0x00;
constexpr uint8_t kStatus2 = 0x01;
constexpr uint8_t kChipId = 0x03;
constexpr uint8_t kInterruptEnable2 = 0x41;
constexpr uint8_t kInterruptStatus2 = 0x49;
constexpr uint8_t kBatteryPercent = 0xA4;
constexpr uint8_t kPowerKeyBits = 0x0C;

}  // namespace

esp_err_t PowerManager::read_register(uint8_t address, uint8_t &value) {
    return i2c_master_transmit_receive(device_, &address, 1, &value, 1,
                                       1000);
}

esp_err_t PowerManager::write_register(uint8_t address, uint8_t value) {
    const uint8_t bytes[] = {address, value};
    return i2c_master_transmit(device_, bytes, sizeof(bytes), 1000);
}

esp_err_t PowerManager::begin() {
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    i2c_device_config_t config{};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = kAddress;
    config.scl_speed_hz = 100'000;
    esp_err_t result = i2c_master_bus_add_device(bus, &config, &device_);
    if (result != ESP_OK) {
        return result;
    }
    uint8_t chip_id = 0;
    result = read_register(kChipId, chip_id);
    if (result != ESP_OK || chip_id != kExpectedChipId) {
        ESP_LOGE(kTag, "AXP2101 probe failed (id=0x%02x)", chip_id);
        device_ = nullptr;
        return result == ESP_OK ? ESP_ERR_NOT_FOUND : result;
    }

    // The board does not wire AXP IRQ to the ESP32, so enable and poll only
    // the two PEK status bits.  Long press remains owned by the PMIC hardware.
    uint8_t interrupt_enable = 0;
    if (read_register(kInterruptEnable2, interrupt_enable) == ESP_OK) {
        result = write_register(kInterruptEnable2,
                                static_cast<uint8_t>(interrupt_enable | kPowerKeyBits));
    }
    if (result == ESP_OK) {
        result = write_register(kInterruptStatus2, kPowerKeyBits);
    }
    ESP_LOGI(kTag, "AXP2101 detected on shared BSP I2C bus");
    return result;
}

esp_err_t PowerManager::read_state(PowerState &state) {
    if (device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t status1 = 0;
    uint8_t status2 = 0;
    esp_err_t result = read_register(kStatus1, status1);
    result |= read_register(kStatus2, status2);
    if (result != ESP_OK) {
        return result;
    }
    const bool battery_connected = (status1 & (1U << 3)) != 0;
    const bool vbus_good = (status1 & (1U << 5)) != 0;
    state.usb_present = vbus_good && (status2 & (1U << 3)) == 0;
    state.battery_present = battery_connected;
    state.charging = (status2 >> 5) == 0x01;
    uint8_t percent = 0;
    if (battery_connected && read_register(kBatteryPercent, percent) == ESP_OK) {
        state.battery_percent = std::min<uint8_t>(percent, 100);
    } else {
        state.battery_percent = 0;
    }
    return ESP_OK;
}

PowerKeyEvent PowerManager::poll_key() {
    if (device_ == nullptr) {
        return PowerKeyEvent::kNone;
    }
    uint8_t status = 0;
    if (read_register(kInterruptStatus2, status) != ESP_OK) {
        return PowerKeyEvent::kNone;
    }
    const uint8_t pending = status & kPowerKeyBits;
    if (pending == 0) {
        return PowerKeyEvent::kNone;
    }
    (void)write_register(kInterruptStatus2, pending);
    if ((pending & (1U << 2)) != 0) {
        return PowerKeyEvent::kLong;
    }
    return (pending & (1U << 3)) != 0 ? PowerKeyEvent::kShort
                                      : PowerKeyEvent::kNone;
}

}  // namespace codex_island::power
