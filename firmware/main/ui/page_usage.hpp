#pragma once

#include <array>

#include "lvgl.h"
#include "model/app_state.hpp"
#include "ui/widgets/gradient_arc.hpp"
#include "ui/widgets/status_icons.hpp"

namespace codex_island::ui {

class UsagePage {
public:
    void create(lv_obj_t *parent);
    void update(const AppState &state);

private:
    GradientArc five_hour_arc_{};
    GradientArc seven_day_arc_{};
    BatteryIcon battery_{};
    lv_obj_t *link_ = nullptr;
    lv_obj_t *waiting_ = nullptr;
    lv_obj_t *five_hour_percent_ = nullptr;
    lv_obj_t *five_hour_label_ = nullptr;
    lv_obj_t *five_hour_detail_ = nullptr;
    lv_obj_t *seven_day_percent_ = nullptr;
    lv_obj_t *seven_day_label_ = nullptr;
    lv_obj_t *seven_day_detail_ = nullptr;
    lv_obj_t *tokens_ = nullptr;
    lv_obj_t *cost_ = nullptr;
    std::array<lv_obj_t *, kDailyPoints> bars_{};
};

}  // namespace codex_island::ui
