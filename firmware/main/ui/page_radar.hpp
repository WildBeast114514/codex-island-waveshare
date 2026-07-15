#pragma once

#include <array>

#include "lvgl.h"
#include "model/app_state.hpp"

namespace codex_island::ui {

class RadarPage {
public:
    void create(lv_obj_t *parent);
    void update(const AppState &state);

private:
    void render_rows();

    RadarState state_{};
    lv_obj_t *updated_ = nullptr;
    lv_obj_t *waiting_ = nullptr;
    lv_obj_t *list_ = nullptr;
    std::array<lv_obj_t *, kMaxRadarModels> dots_{};
    std::array<lv_obj_t *, kMaxRadarModels> names_{};
    std::array<lv_obj_t *, kMaxRadarModels> scores_{};
    std::array<uint8_t, kMaxRadarModels> order_{};
};

}  // namespace codex_island::ui
