#include "ui/widgets/gradient_arc.hpp"

#include <algorithm>
#include <cmath>

#include "ui/ui_theme.hpp"

namespace codex_island::ui {
namespace {

constexpr int kStartAngle = 135;
constexpr int kSweepAngle = 270;
constexpr int kArcWidth = 10;

uint8_t channel(uint32_t color, int shift) {
    return static_cast<uint8_t>((color >> shift) & 0xFFU);
}

uint32_t blend(uint32_t from, uint32_t to, int amount, int range) {
    auto mix = [amount, range](int a, int b) {
        return static_cast<uint8_t>(a + ((b - a) * amount) / range);
    };
    return (static_cast<uint32_t>(mix(channel(from, 16), channel(to, 16))) << 16) |
           (static_cast<uint32_t>(mix(channel(from, 8), channel(to, 8))) << 8) |
           static_cast<uint32_t>(mix(channel(from, 0), channel(to, 0)));
}

uint32_t gradient_color(std::size_t index, std::size_t count) {
    if (count <= 1) {
        return kGreen;
    }
    const int position = static_cast<int>((index * 200) / (count - 1));
    if (position <= 100) {
        return blend(kBlue, kCyan, position, 100);
    }
    return blend(kCyan, kGreen, position - 100, 100);
}

void strip_arc_interaction(lv_obj_t *arc) {
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(arc, 0, 0);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
}

}  // namespace

void GradientArc::create(lv_obj_t *parent, int x, int y, int size) {
    parent_ = parent;
    x_ = x;
    y_ = y;
    size_ = size;

    track_ = lv_arc_create(parent);
    lv_obj_set_size(track_, size, size);
    lv_obj_set_pos(track_, x, y);
    lv_arc_set_bg_angles(track_, kStartAngle, (kStartAngle + kSweepAngle) % 360);
    lv_arc_set_angles(track_, kStartAngle, (kStartAngle + kSweepAngle) % 360);
    strip_arc_interaction(track_);
    lv_obj_set_style_arc_color(track_, lv_color_hex(kDarkRing), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(track_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(track_, kArcWidth, LV_PART_MAIN);
    lv_obj_set_style_arc_color(track_, lv_color_hex(kDarkRing), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(track_, kArcWidth, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(track_, true, LV_PART_MAIN);

    for (std::size_t i = 0; i < kSegments; ++i) {
        const int start = (kStartAngle + static_cast<int>((i * kSweepAngle) / kSegments)) % 360;
        const int end = (kStartAngle + static_cast<int>(((i + 1) * kSweepAngle) / kSegments) + 1) % 360;
        lv_obj_t *segment = lv_arc_create(parent);
        lv_obj_set_size(segment, size, size);
        lv_obj_set_pos(segment, x, y);
        lv_arc_set_bg_angles(segment, start, end);
        lv_arc_set_angles(segment, start, end);
        strip_arc_interaction(segment);
        lv_obj_set_style_arc_width(segment, kArcWidth, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(segment, false, LV_PART_INDICATOR);
        segments_[i] = segment;
    }

    endpoint_ = make_dot(parent, x, y, 18, kGreen);
    set_value(0);
}

void GradientArc::set_value(uint8_t percent) {
    const auto clamped = static_cast<uint8_t>(std::min<int>(percent, 100));
    const std::size_t visible = std::max<std::size_t>(1, (clamped * kSegments) / 100);
    for (std::size_t i = 0; i < kSegments; ++i) {
        if (clamped > 0 && i < visible) {
            lv_obj_clear_flag(segments_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_arc_color(segments_[i], lv_color_hex(gradient_color(i, visible)),
                                       LV_PART_INDICATOR);
        } else {
            lv_obj_add_flag(segments_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (clamped == 0) {
        lv_obj_add_flag(endpoint_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(endpoint_, LV_OBJ_FLAG_HIDDEN);
    const double angle = (kStartAngle + (kSweepAngle * clamped) / 100.0) * 3.14159265358979323846 / 180.0;
    const double radius = (size_ - kArcWidth) / 2.0;
    const int center_x = x_ + size_ / 2;
    const int center_y = y_ + size_ / 2;
    const int point_x = center_x + static_cast<int>(std::cos(angle) * radius) - 9;
    const int point_y = center_y + static_cast<int>(std::sin(angle) * radius) - 9;
    lv_obj_set_pos(endpoint_, point_x, point_y);
    lv_obj_move_foreground(endpoint_);
}

}  // namespace codex_island::ui
