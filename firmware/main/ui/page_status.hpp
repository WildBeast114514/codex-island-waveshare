#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "lvgl.h"
#include "model/app_state.hpp"

namespace codex_island::ui {

class StatusPage {
public:
    void create(lv_obj_t *parent);
    void update(const AppState &state, int64_t monotonic_seconds);
    void set_brightness(uint8_t brightness);
    bool take_brightness_request(uint8_t &brightness);

private:
    static void brightness_event(lv_event_t *event);

    lv_obj_t *sync_age_ = nullptr;
    lv_obj_t *stale_value_ = nullptr;
    lv_obj_t *ble_value_ = nullptr;
    lv_obj_t *power_value_ = nullptr;
    lv_obj_t *trend_ = nullptr;
    lv_obj_t *brightness_slider_ = nullptr;
    lv_obj_t *brightness_value_ = nullptr;
    std::atomic<int16_t> brightness_request_{-1};
    std::array<lv_point_precise_t, kTrendPoints> trend_points_{};
};

}  // namespace codex_island::ui
