#pragma once

#include <cstdint>

namespace codex_island::input {

struct ButtonEvents {
    bool short_press = false;
    bool double_press = false;
    bool long_press = false;
};

class ButtonStateMachine {
public:
    ButtonEvents update(bool pressed, uint32_t now_ms);

private:
    bool raw_pressed_ = false;
    bool stable_pressed_ = false;
    bool long_emitted_ = false;
    bool short_pending_ = false;
    uint32_t raw_changed_at_ = 0;
    uint32_t pressed_at_ = 0;
    uint32_t first_release_at_ = 0;
};

}  // namespace codex_island::input
