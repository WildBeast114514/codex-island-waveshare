#pragma once

#include <array>

#include "lvgl.h"
#include "model/app_state.hpp"

namespace codex_island::ui {

class DistributedRadarPage {
public:
    void create(lv_obj_t *parent);
    void update(const AppState &state);

private:
    void render_rows();

    DistributedRadarState state_{};
    lv_obj_t *updated_ = nullptr;
    lv_obj_t *waiting_ = nullptr;
    lv_obj_t *list_ = nullptr;
    std::array<lv_obj_t *, kMaxDistributedRows> row_panels_{};
    std::array<lv_obj_t *, kMaxDistributedRows> accents_{};
    std::array<lv_obj_t *, kMaxDistributedRows> names_{};
    std::array<lv_obj_t *, kMaxDistributedRows> samples_{};
    std::array<lv_obj_t *, kMaxDistributedRows> scores_{};
};

}  // namespace codex_island::ui
