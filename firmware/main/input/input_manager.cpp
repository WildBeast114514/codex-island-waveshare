#include "input/input_manager.hpp"

#include "driver/gpio.h"

namespace codex_island::input {
namespace {

constexpr gpio_num_t kBootPin = GPIO_NUM_0;
}  // namespace

esp_err_t InputManager::begin() {
    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << kBootPin;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&config);
}

ButtonEvents InputManager::poll(uint32_t now_ms) {
    return boot_.update(gpio_get_level(kBootPin) == 0, now_ms);
}

}  // namespace codex_island::input
