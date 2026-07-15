#include "ui/page_radar.hpp"

#include <algorithm>
#include <cstdlib>

#include "ui/ui_theme.hpp"
#include "ui/widgets/status_icons.hpp"

namespace codex_island::ui {
namespace {

constexpr std::array<uint32_t, 5> kRankColors = {
    kGreen, kLime, kBlue, kPurple, kOrange};
constexpr int kRowHeight = 43;

}  // namespace

void RadarPage::create(lv_obj_t *parent) {
    style_black_surface(parent);
    create_badge(parent, 218, 17, 30, kBlue, "o", &lv_font_montserrat_14);
    lv_obj_t *title =
        make_label(parent, "CODEX RADAR", &lv_font_montserrat_22, kWhite);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 54);
    updated_ =
        make_label(parent, "UPDATED --", &lv_font_montserrat_12, kMuted);
    lv_obj_align(updated_, LV_ALIGN_TOP_MID, 0, 86);

    waiting_ = make_label(parent, "Waiting for radar data",
                          &lv_font_montserrat_18, kMuted);
    lv_obj_align(waiting_, LV_ALIGN_CENTER, 0, -5);

    list_ = lv_obj_create(parent);
    lv_obj_set_size(list_, 362, 310);
    lv_obj_set_pos(list_, 52, 108);
    style_black_surface(list_);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(list_, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list_, lv_color_hex(kBlue), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list_, LV_OPA_70, LV_PART_SCROLLBAR);
    lv_obj_add_flag(list_, static_cast<lv_obj_flag_t>(
                               LV_OBJ_FLAG_SCROLL_CHAIN_HOR |
                               LV_OBJ_FLAG_GESTURE_BUBBLE));

    for (std::size_t row = 0; row < kMaxRadarModels; ++row) {
        const int y = static_cast<int>(row) * kRowHeight;
        const uint32_t color = kRankColors[row % kRankColors.size()];
        dots_[row] = make_dot(list_, 4, y + 15, 10, color);
        names_[row] =
            make_label(list_, "", &lv_font_montserrat_18, kWhite);
        lv_obj_set_pos(names_[row], 22, y + 8);
        lv_obj_set_width(names_[row], 238);
        lv_label_set_long_mode(names_[row], LV_LABEL_LONG_DOT);
        scores_[row] =
            make_label(list_, "", &lv_font_montserrat_18, color);
        lv_obj_set_width(scores_[row], 72);
        lv_obj_set_style_text_align(scores_[row], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(scores_[row], 270, y + 8);
        lv_obj_add_flag(dots_[row], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(names_[row], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scores_[row], LV_OBJ_FLAG_HIDDEN);
    }
}

void RadarPage::update(const AppState &state) {
    state_ = state.radar;
    const auto count =
        std::min<std::size_t>(state_.count, kMaxRadarModels);
    for (std::size_t i = 0; i < count; ++i) {
        order_[i] = static_cast<uint8_t>(i);
    }
    std::stable_sort(order_.begin(), order_.begin() + count,
                     [this](uint8_t a, uint8_t b) {
                         return state_.models[a].iq_x10 >
                                state_.models[b].iq_x10;
                     });
    render_rows();
}

void RadarPage::render_rows() {
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

    for (std::size_t row = 0; row < kMaxRadarModels; ++row) {
        const bool visible = state_.valid && row < state_.count;
        lv_obj_set_flag(dots_[row], LV_OBJ_FLAG_HIDDEN, !visible);
        lv_obj_set_flag(names_[row], LV_OBJ_FLAG_HIDDEN, !visible);
        lv_obj_set_flag(scores_[row], LV_OBJ_FLAG_HIDDEN, !visible);
        if (!visible) {
            continue;
        }

        const RadarModel &model = state_.models[order_[row]];
        lv_label_set_text_fmt(names_[row], "%s %s", model.family,
                              model.effort);
        if ((model.iq_x10 % 10) == 0) {
            lv_label_set_text_fmt(scores_[row], "%d", model.iq_x10 / 10);
        } else {
            lv_label_set_text_fmt(scores_[row], "%d.%d", model.iq_x10 / 10,
                                  std::abs(model.iq_x10 % 10));
        }
    }
}

}  // namespace codex_island::ui
