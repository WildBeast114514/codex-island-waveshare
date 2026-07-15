#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "lvgl.h"

namespace codex_island::ui {

class GradientArc {
public:
    void create(lv_obj_t *parent, int x, int y, int size,
                uint32_t start_color = 0x1684FF,
                uint32_t middle_color = 0x00C8D7,
                uint32_t end_color = 0x79F14A);
    void set_value(uint8_t percent);

private:
    static constexpr std::size_t kSegments = 90;
    lv_obj_t *parent_ = nullptr;
    lv_obj_t *track_ = nullptr;
    lv_obj_t *endpoint_ = nullptr;
    std::array<lv_obj_t *, kSegments> segments_{};
    int x_ = 0;
    int y_ = 0;
    int size_ = 0;
    uint32_t start_color_ = 0;
    uint32_t middle_color_ = 0;
    uint32_t end_color_ = 0;
};

}  // namespace codex_island::ui
