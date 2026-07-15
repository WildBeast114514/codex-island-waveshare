#pragma once

#include <array>
#include <cstddef>

#include "lvgl.h"

namespace codex_island::ui {

class GradientArc {
public:
    void create(lv_obj_t *parent, int x, int y, int size);
    void set_value(uint8_t percent);

private:
    static constexpr std::size_t kSegments = 120;
    lv_obj_t *parent_ = nullptr;
    lv_obj_t *track_ = nullptr;
    lv_obj_t *endpoint_ = nullptr;
    std::array<lv_obj_t *, kSegments> segments_{};
    int x_ = 0;
    int y_ = 0;
    int size_ = 0;
};

}  // namespace codex_island::ui
