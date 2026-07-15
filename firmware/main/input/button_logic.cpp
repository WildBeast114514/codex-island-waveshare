#include "input/button_logic.hpp"

namespace codex_island::input {
namespace {

constexpr uint32_t kDebounceMs = 30;
constexpr uint32_t kDoubleMs = 400;
constexpr uint32_t kLongMs = 1200;

}  // namespace

ButtonEvents ButtonStateMachine::update(bool pressed, uint32_t now_ms) {
    ButtonEvents events{};
    if (pressed != raw_pressed_) {
        raw_pressed_ = pressed;
        raw_changed_at_ = now_ms;
    }
    if (raw_pressed_ != stable_pressed_ && now_ms - raw_changed_at_ >= kDebounceMs) {
        stable_pressed_ = raw_pressed_;
        if (stable_pressed_) {
            pressed_at_ = now_ms;
            long_emitted_ = false;
        } else if (!long_emitted_) {
            if (short_pending_ && now_ms - first_release_at_ <= kDoubleMs) {
                short_pending_ = false;
                events.double_press = true;
            } else {
                short_pending_ = true;
                first_release_at_ = now_ms;
            }
        }
    }
    if (stable_pressed_ && !long_emitted_ && now_ms - pressed_at_ >= kLongMs) {
        long_emitted_ = true;
        short_pending_ = false;
        events.long_press = true;
    }
    if (!stable_pressed_ && short_pending_ && now_ms - first_release_at_ > kDoubleMs) {
        short_pending_ = false;
        events.short_press = true;
    }
    return events;
}

}  // namespace codex_island::input
