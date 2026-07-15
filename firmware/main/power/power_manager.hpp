#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "model/app_state.hpp"

namespace codex_island::power {

enum class PowerKeyEvent : uint8_t {
    kNone,
    kShort,
    kLong,
};

class PowerManager {
public:
    esp_err_t begin();
    esp_err_t read_state(PowerState &state);
    PowerKeyEvent poll_key();
    bool ready() const { return device_ != nullptr; }

private:
    esp_err_t read_register(uint8_t address, uint8_t &value);
    esp_err_t write_register(uint8_t address, uint8_t value);

    i2c_master_dev_handle_t device_ = nullptr;
};

}  // namespace codex_island::power
