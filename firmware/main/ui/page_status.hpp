#pragma once

#include <array>

#include "lvgl.h"
#include "model/app_state.hpp"

namespace codex_island::ui {

class StatusPage {
public:
    void create(lv_obj_t *parent);
    void update(const AppState &state, int64_t monotonic_seconds);

private:
    lv_obj_t *sync_age_ = nullptr;
    lv_obj_t *stale_value_ = nullptr;
    lv_obj_t *ble_value_ = nullptr;
    lv_obj_t *power_value_ = nullptr;
    lv_obj_t *trend_ = nullptr;
    std::array<lv_point_precise_t, kTrendPoints> trend_points_{};
};

}  // namespace codex_island::ui
