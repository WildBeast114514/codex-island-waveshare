#include "ui/page_radar.hpp"

#include <algorithm>
#include <cstdlib>

#include "ui/ui_theme.hpp"
#include "ui/widgets/status_icons.hpp"

namespace codex_island::ui {
namespace {

constexpr std::array<uint32_t, 5> kRankColors = {kGreen, kLime, kBlue, kPurple, kOrange};

}  // namespace

void RadarPage::create(lv_obj_t *parent) {
    style_black_surface(parent);
    lv_obj_add_event_cb(parent, gesture_event, LV_EVENT_GESTURE, this);
    create_badge(parent, 216, 24, 34, kBlue, "o", &lv_font_montserrat_16);
    lv_obj_t *title = make_label(parent, "CODEX RADAR", &lv_font_montserrat_22, kWhite);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 67);
    stale_ = make_label(parent, "STALE", &lv_font_montserrat_12, kOrange);
    lv_obj_align(stale_, LV_ALIGN_TOP_MID, 0, 94);
    lv_obj_add_flag(stale_, LV_OBJ_FLAG_HIDDEN);

    waiting_ = make_label(parent, "Waiting for radar data", &lv_font_montserrat_18, kMuted);
    lv_obj_align(waiting_, LV_ALIGN_CENTER, 0, -5);

    for (std::size_t i = 0; i < 5; ++i) {
        const int y = 113 + static_cast<int>(i) * 47;
        dots_[i] = make_dot(parent, 78, y + 5, 10, kRankColors[i]);
        names_[i] = make_label(parent, "", &lv_font_montserrat_18, kWhite);
        lv_obj_set_pos(names_[i], 98, y);
        scores_[i] = make_label(parent, "", &lv_font_montserrat_18, kRankColors[i]);
        lv_obj_set_width(scores_[i], 72);
        lv_obj_set_style_text_align(scores_[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(scores_[i], 301, y);
        lv_obj_add_flag(dots_[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(names_[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scores_[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void RadarPage::gesture_event(lv_event_t *event) {
    auto *page = static_cast<RadarPage *>(lv_event_get_user_data(event));
    lv_indev_t *input = lv_indev_active();
    if (page == nullptr || input == nullptr) {
        return;
    }
    const lv_dir_t direction = lv_indev_get_gesture_dir(input);
    if (direction == LV_DIR_TOP) {
        page->move_group(1);
    } else if (direction == LV_DIR_BOTTOM) {
        page->move_group(-1);
    }
}

void RadarPage::update(const AppState &state) {
    state_ = state.radar;
    group_ = 0;
    const auto count = std::min<std::size_t>(state_.count, kMaxRadarModels);
    for (std::size_t i = 0; i < count; ++i) {
        order_[i] = static_cast<uint8_t>(i);
    }
    std::stable_sort(order_.begin(), order_.begin() + count, [this](uint8_t a, uint8_t b) {
        return state_.models[a].iq_x10 > state_.models[b].iq_x10;
    });
    render_rows();
}

void RadarPage::move_group(int delta) {
    const int groups = std::max(1, (static_cast<int>(state_.count) + 4) / 5);
    group_ = std::clamp(group_ + delta, 0, groups - 1);
    render_rows();
}

void RadarPage::render_rows() {
    lv_obj_set_flag(stale_, LV_OBJ_FLAG_HIDDEN, !state_.stale);
    lv_obj_set_flag(waiting_, LV_OBJ_FLAG_HIDDEN, state_.valid);

    for (std::size_t row = 0; row < 5; ++row) {
        const std::size_t position = static_cast<std::size_t>(group_ * 5) + row;
        const bool visible = state_.valid && position < state_.count;
        lv_obj_set_flag(dots_[row], LV_OBJ_FLAG_HIDDEN, !visible);
        lv_obj_set_flag(names_[row], LV_OBJ_FLAG_HIDDEN, !visible);
        lv_obj_set_flag(scores_[row], LV_OBJ_FLAG_HIDDEN, !visible);
        if (!visible) {
            continue;
        }

        const RadarModel &model = state_.models[order_[position]];
        lv_label_set_text_fmt(names_[row], "%s %s", model.family, model.effort);
        if ((model.iq_x10 % 10) == 0) {
            lv_label_set_text_fmt(scores_[row], "%d", model.iq_x10 / 10);
        } else {
            lv_label_set_text_fmt(scores_[row], "%d.%d", model.iq_x10 / 10,
                                  std::abs(model.iq_x10 % 10));
        }
    }
}

}  // namespace codex_island::ui
