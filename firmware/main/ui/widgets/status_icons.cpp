#include "ui/widgets/status_icons.hpp"

#include <algorithm>

#include "ui/ui_theme.hpp"

namespace codex_island::ui {

void BatteryIcon::create(lv_obj_t *parent, int x, int y) {
    outline = lv_obj_create(parent);
    lv_obj_set_size(outline, 29, 15);
    lv_obj_set_pos(outline, x, y);
    lv_obj_set_style_radius(outline, 3, 0);
    lv_obj_set_style_bg_opa(outline, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(outline, 2, 0);
    lv_obj_set_style_border_color(outline, lv_color_hex(kWhite), 0);
    lv_obj_set_style_pad_all(outline, 2, 0);
    lv_obj_clear_flag(outline, LV_OBJ_FLAG_SCROLLABLE);

    fill = lv_obj_create(outline);
    lv_obj_set_height(fill, 7);
    lv_obj_align(fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(fill, 1, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);

    terminal = lv_obj_create(parent);
    lv_obj_set_size(terminal, 3, 7);
    lv_obj_set_pos(terminal, x + 30, y + 4);
    lv_obj_set_style_radius(terminal, 1, 0);
    lv_obj_set_style_border_width(terminal, 0, 0);
    lv_obj_set_style_bg_color(terminal, lv_color_hex(kWhite), 0);
    lv_obj_set_style_bg_opa(terminal, LV_OPA_COVER, 0);
    lv_obj_clear_flag(terminal, LV_OBJ_FLAG_SCROLLABLE);
}

void BatteryIcon::update(uint8_t percent, bool charging) {
    const int width = std::max(2, (static_cast<int>(std::min<uint8_t>(percent, 100)) * 19) / 100);
    const uint32_t color = percent < 10 ? kOrange : (charging ? kCyan : kGreen);
    lv_obj_set_width(fill, width);
    lv_obj_set_style_bg_color(fill, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
}

lv_obj_t *create_badge(lv_obj_t *parent, int x, int y, int diameter,
                       uint32_t color, const char *glyph, const lv_font_t *font) {
    lv_obj_t *badge = lv_obj_create(parent);
    lv_obj_set_size(badge, diameter, diameter);
    lv_obj_set_pos(badge, x, y);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(badge, 2, 0);
    lv_obj_set_style_border_color(badge, lv_color_hex(color), 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    if (glyph != nullptr && glyph[0] != '\0') {
        lv_obj_t *label = make_label(badge, glyph, font, color);
        lv_obj_center(label);
    }
    return badge;
}

}  // namespace codex_island::ui
