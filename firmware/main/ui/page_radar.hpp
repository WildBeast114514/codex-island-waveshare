#pragma once

#include <array>

#include "lvgl.h"
#include "model/app_state.hpp"

namespace codex_island::ui {

class RadarPage {
public:
    void create(lv_obj_t *parent);
    void update(const AppState &state);
    void move_group(int delta);

private:
    void render_rows();

    RadarState state_{};
    int group_ = 0;
    lv_obj_t *stale_ = nullptr;
    lv_obj_t *waiting_ = nullptr;
    std::array<lv_obj_t *, 5> dots_{};
    std::array<lv_obj_t *, 5> names_{};
    std::array<lv_obj_t *, 5> scores_{};
    std::array<uint8_t, kMaxRadarModels> order_{};
};

}  // namespace codex_island::ui
