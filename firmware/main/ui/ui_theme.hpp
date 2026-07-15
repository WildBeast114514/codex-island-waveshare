#pragma once

#include <cstdint>

#include "lvgl.h"

namespace codex_island::ui {

constexpr uint32_t kBlack = 0x000000;
constexpr uint32_t kWhite = 0xF7F7F7;
constexpr uint32_t kMuted = 0x98A6B8;
constexpr uint32_t kDivider = 0x18212B;
constexpr uint32_t kBlue = 0x1684FF;
constexpr uint32_t kCyan = 0x00C8D7;
constexpr uint32_t kGreen = 0x79F14A;
constexpr uint32_t kLime = 0xB9F35A;
constexpr uint32_t kPurple = 0xA84EFF;
constexpr uint32_t kMagenta = 0xE94BEA;
constexpr uint32_t kOrange = 0xFFB000;
constexpr uint32_t kDarkRing = 0x10231A;

void style_black_surface(lv_obj_t *object);
lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                     uint32_t color);
lv_obj_t *make_divider(lv_obj_t *parent, int width, int x, int y);
lv_obj_t *make_dot(lv_obj_t *parent, int x, int y, int diameter, uint32_t color);

}  // namespace codex_island::ui
