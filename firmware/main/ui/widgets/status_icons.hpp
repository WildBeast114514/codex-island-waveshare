#pragma once

#include "lvgl.h"

namespace codex_island::ui {

struct BatteryIcon {
    lv_obj_t *outline = nullptr;
    lv_obj_t *fill = nullptr;
    lv_obj_t *terminal = nullptr;

    void create(lv_obj_t *parent, int x, int y);
    void update(uint8_t percent, bool charging, bool present = true);
};

lv_obj_t *create_badge(lv_obj_t *parent, int x, int y, int diameter,
                       uint32_t color, const char *glyph, const lv_font_t *font);

}  // namespace codex_island::ui
