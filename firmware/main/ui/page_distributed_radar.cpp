#include "ui/page_distributed_radar.hpp"

#include <algorithm>

#include "ui/ui_theme.hpp"

namespace codex_island::ui {
namespace {

constexpr std::array<uint32_t, 5> kGroupColors = {
    kOrange, kBlue, 0xC7D2E0, kCyan, kMagenta};
constexpr int kAggregateHeight = 46;
constexpr int kEffortHeight = 38;

lv_obj_t *make_scope(lv_obj_t *parent) {
    lv_obj_t *outer = lv_obj_create(parent);
    lv_obj_set_size(outer, 34, 34);
    lv_obj_set_pos(outer, 216, 13);
    lv_obj_set_style_radius(outer, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(outer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(outer, 2, 0);
    lv_obj_set_style_border_color(outer, lv_color_hex(kCyan), 0);
    lv_obj_set_style_pad_all(outer, 0, 0);
    lv_obj_clear_flag(outer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *inner = lv_obj_create(outer);
    lv_obj_set_size(inner, 16, 16);
    lv_obj_center(inner);
    lv_obj_set_style_radius(inner, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(inner, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(inner, 1, 0);
    lv_obj_set_style_border_color(inner, lv_color_hex(kCyan), 0);
    lv_obj_set_style_border_opa(inner, LV_OPA_50, 0);
    lv_obj_set_style_pad_all(inner, 0, 0);
    lv_obj_clear_flag(inner, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *beam = lv_obj_create(outer);
    lv_obj_set_size(beam, 13, 2);
    lv_obj_set_pos(beam, 16, 15);
    lv_obj_set_style_bg_color(beam, lv_color_hex(kCyan), 0);
    lv_obj_set_style_bg_opa(beam, LV_OPA_70, 0);
    lv_obj_set_style_border_width(beam, 0, 0);
    lv_obj_set_style_pad_all(beam, 0, 0);
    lv_obj_clear_flag(beam, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *blip = make_dot(outer, 23, 6, 6, kGreen);
    lv_obj_set_style_shadow_width(blip, 8, 0);
    lv_obj_set_style_shadow_color(blip, lv_color_hex(kGreen), 0);
    lv_obj_set_style_shadow_opa(blip, LV_OPA_60, 0);
    return outer;
}

}  // namespace

void DistributedRadarPage::create(lv_obj_t *parent) {
    style_black_surface(parent);
    make_scope(parent);
    lv_obj_t *title =
        make_label(parent, "DISTRIBUTED IQ", &lv_font_montserrat_22, kWhite);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 52);
    updated_ = make_label(parent, "UPDATED --", &lv_font_montserrat_12, kMuted);
    lv_obj_align(updated_, LV_ALIGN_TOP_MID, 0, 84);

    waiting_ = make_label(parent, "Waiting for live IQ",
                          &lv_font_montserrat_18, kMuted);
    lv_obj_align(waiting_, LV_ALIGN_CENTER, 0, -3);

    list_ = lv_obj_create(parent);
    lv_obj_set_size(list_, 372, 312);
    lv_obj_set_pos(list_, 47, 108);
    style_black_surface(list_);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(list_, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list_, lv_color_hex(kCyan), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list_, LV_OPA_70, LV_PART_SCROLLBAR);
    lv_obj_add_flag(list_, static_cast<lv_obj_flag_t>(
                               LV_OBJ_FLAG_SCROLL_CHAIN_HOR |
                               LV_OBJ_FLAG_GESTURE_BUBBLE));

    for (std::size_t row = 0; row < kMaxDistributedRows; ++row) {
        row_panels_[row] = lv_obj_create(list_);
        lv_obj_set_size(row_panels_[row], 360, kEffortHeight);
        lv_obj_set_pos(row_panels_[row], 2,
                       static_cast<int>(row) * kEffortHeight);
        lv_obj_set_style_radius(row_panels_[row], 8, 0);
        lv_obj_set_style_bg_opa(row_panels_[row], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_panels_[row], 0, 0);
        lv_obj_set_style_pad_all(row_panels_[row], 0, 0);
        lv_obj_clear_flag(row_panels_[row], LV_OBJ_FLAG_SCROLLABLE);

        accents_[row] = lv_obj_create(row_panels_[row]);
        lv_obj_set_size(accents_[row], 6, 6);
        lv_obj_set_pos(accents_[row], 18, 16);
        lv_obj_set_style_radius(accents_[row], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(accents_[row], 0, 0);
        lv_obj_set_style_pad_all(accents_[row], 0, 0);
        lv_obj_clear_flag(accents_[row], LV_OBJ_FLAG_SCROLLABLE);

        names_[row] =
            make_label(row_panels_[row], "", &lv_font_montserrat_18, kWhite);
        lv_obj_set_pos(names_[row], 34, 8);
        lv_obj_set_width(names_[row], 130);
        lv_label_set_long_mode(names_[row], LV_LABEL_LONG_DOT);

        samples_[row] =
            make_label(row_panels_[row], "", &lv_font_montserrat_12, kMuted);
        lv_obj_set_pos(samples_[row], 168, 12);
        lv_obj_set_width(samples_[row], 92);
        lv_obj_set_style_text_align(samples_[row], LV_TEXT_ALIGN_RIGHT, 0);

        scores_[row] =
            make_label(row_panels_[row], "", &lv_font_montserrat_18, kCyan);
        lv_obj_set_pos(scores_[row], 276, 8);
        lv_obj_set_width(scores_[row], 64);
        lv_obj_set_style_text_align(scores_[row], LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_add_flag(row_panels_[row], LV_OBJ_FLAG_HIDDEN);
    }
}

void DistributedRadarPage::update(const AppState &state) {
    state_ = state.distributed_radar;
    render_rows();
}

void DistributedRadarPage::render_rows() {
    lv_obj_set_flag(waiting_, LV_OBJ_FLAG_HIDDEN, state_.valid);
    lv_obj_set_flag(list_, LV_OBJ_FLAG_HIDDEN, !state_.valid);
    if (state_.valid) {
        lv_label_set_text_fmt(updated_, "UPDATED %s%s", state_.updated_label,
                              state_.stale ? "  STALE" : "");
        lv_obj_set_style_text_color(
            updated_, lv_color_hex(state_.stale ? kOrange : kMuted), 0);
    } else {
        lv_label_set_text(updated_, "UPDATED --");
        lv_obj_set_style_text_color(updated_, lv_color_hex(kMuted), 0);
    }

    const std::size_t count =
        std::min<std::size_t>(state_.count, kMaxDistributedRows);
    int y = 0;
    std::size_t group = 0;
    for (std::size_t row = 0; row < kMaxDistributedRows; ++row) {
        const bool visible = state_.valid && row < count;
        lv_obj_set_flag(row_panels_[row], LV_OBJ_FLAG_HIDDEN, !visible);
        if (!visible) {
            continue;
        }

        const DistributedRadarRow &value = state_.rows[row];
        if (value.aggregate && row > 0) {
            ++group;
            y += 4;
        }
        const uint32_t color = kGroupColors[group % kGroupColors.size()];
        const int height = value.aggregate ? kAggregateHeight : kEffortHeight;
        lv_obj_set_pos(row_panels_[row], 2, y);
        lv_obj_set_height(row_panels_[row], height);
        lv_obj_set_style_bg_color(row_panels_[row], lv_color_hex(0x071512), 0);
        lv_obj_set_style_bg_opa(row_panels_[row],
                                value.aggregate ? LV_OPA_80 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_panels_[row],
                                      value.aggregate ? 1 : 0, 0);
        lv_obj_set_style_border_color(row_panels_[row], lv_color_hex(color), 0);
        lv_obj_set_style_border_opa(row_panels_[row], LV_OPA_40, 0);

        lv_obj_set_style_bg_color(accents_[row], lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(accents_[row], LV_OPA_COVER, 0);
        if (value.aggregate) {
            lv_obj_set_size(accents_[row], 4, 28);
            lv_obj_set_pos(accents_[row], 11, 9);
            lv_obj_set_style_radius(accents_[row], 2, 0);
            lv_obj_set_pos(names_[row], 24, 11);
            lv_obj_set_style_text_color(names_[row], lv_color_hex(color), 0);
            lv_label_set_text(names_[row], value.model);
            lv_obj_set_pos(samples_[row], 168, 15);
            lv_obj_set_pos(scores_[row], 276, 11);
        } else {
            lv_obj_set_size(accents_[row], 6, 6);
            lv_obj_set_pos(accents_[row], 22, 16);
            lv_obj_set_style_radius(accents_[row], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_pos(names_[row], 42, 8);
            lv_obj_set_style_text_color(names_[row], lv_color_hex(kWhite), 0);
            lv_label_set_text(names_[row], value.effort);
            lv_obj_set_pos(samples_[row], 168, 12);
            lv_obj_set_pos(scores_[row], 276, 8);
        }
        lv_obj_set_style_text_color(scores_[row], lv_color_hex(color), 0);
        if (value.has_data) {
            lv_label_set_text_fmt(samples_[row], "%u/%u",
                                  static_cast<unsigned>(value.passed),
                                  static_cast<unsigned>(value.total));
            lv_label_set_text_fmt(scores_[row], "%u",
                                  static_cast<unsigned>(value.iq));
        } else {
            lv_label_set_text(samples_[row], "--");
            lv_label_set_text(scores_[row], "--");
        }
        y += height;
    }
}

}  // namespace codex_island::ui
