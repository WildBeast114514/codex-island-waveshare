#pragma once

#include <cstdint>

#include "esp_err.h"
#include "input/button_logic.hpp"

namespace codex_island::input {

class InputManager {
public:
    esp_err_t begin();
    ButtonEvents poll(uint32_t now_ms);

private:
    ButtonStateMachine boot_{};
};

}  // namespace codex_island::input
