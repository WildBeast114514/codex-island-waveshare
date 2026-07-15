#include "ui/ui_theme.hpp"

namespace codex_island::ui {

void style_black_surface(lv_obj_t *object) {
    lv_obj_set_style_bg_color(object, lv_color_hex(kBlack), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_style_radius(object, 0, 0);
}

lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                     uint32_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    return label;
}

lv_obj_t *make_divider(lv_obj_t *parent, int width, int x, int y) {
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, width, 1);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_style_bg_color(line, lv_color_hex(kDivider), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    return line;
}

lv_obj_t *make_dot(lv_obj_t *parent, int x, int y, int diameter, uint32_t color) {
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, diameter, diameter);
    lv_obj_set_pos(dot, x, y);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    return dot;
}

}  // namespace codex_island::ui
