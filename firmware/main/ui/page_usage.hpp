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
    GradientArc arc_{};
    BatteryIcon battery_{};
    lv_obj_t *link_ = nullptr;
    lv_obj_t *waiting_ = nullptr;
    lv_obj_t *percent_ = nullptr;
    lv_obj_t *weekly_ = nullptr;
    lv_obj_t *reset_ = nullptr;
    lv_obj_t *tokens_ = nullptr;
    lv_obj_t *cost_ = nullptr;
    std::array<lv_obj_t *, kDailyPoints> bars_{};
};

}  // namespace codex_island::ui
