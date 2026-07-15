#include "ui/page_status.hpp"

#include <algorithm>

#include "ui/ui_theme.hpp"
#include "ui/widgets/status_icons.hpp"

namespace codex_island::ui {

void StatusPage::create(lv_obj_t *parent) {
    style_black_surface(parent);
    create_badge(parent, 216, 22, 36, kGreen, "~", &lv_font_montserrat_20);
    lv_obj_t *title = make_label(parent, "STATUS", &lv_font_montserrat_22, kWhite);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 67);

    create_badge(parent, 102, 114, 42, kGreen, LV_SYMBOL_OK, &lv_font_montserrat_20);
    lv_obj_t *sync = make_label(parent, "Last sync", &lv_font_montserrat_18, kWhite);
    lv_obj_set_pos(sync, 161, 112);
    sync_age_ = make_label(parent, "12m ago", &lv_font_montserrat_20, kGreen);
    lv_obj_set_pos(sync_age_, 161, 136);

    make_divider(parent, 252, 107, 182);
    create_badge(parent, 112, 208, 36, kPurple, "!", &lv_font_montserrat_18);
    lv_obj_t *stale = make_label(parent, "stale:", &lv_font_montserrat_16, kWhite);
    lv_obj_set_pos(stale, 164, 211);
    stale_value_ = make_label(parent, "no", &lv_font_montserrat_16, kBlue);
    lv_obj_set_pos(stale_value_, 222, 211);

    lv_obj_t *ble = make_label(parent, "BLE", &lv_font_montserrat_14, kMuted);
    lv_obj_set_pos(ble, 127, 259);
    ble_value_ = make_label(parent, "connected", &lv_font_montserrat_14, kGreen);
    lv_obj_set_pos(ble_value_, 166, 259);
    lv_obj_t *power = make_label(parent, "POWER", &lv_font_montserrat_14, kMuted);
    lv_obj_set_pos(power, 127, 282);
    power_value_ = make_label(parent, "USB 86%", &lv_font_montserrat_14, kWhite);
    lv_obj_set_pos(power_value_, 190, 282);

    trend_ = lv_line_create(parent);
    lv_obj_set_pos(trend_, 140, 326);
    lv_obj_set_style_line_color(trend_, lv_color_hex(kGreen), 0);
    lv_obj_set_style_line_width(trend_, 3, 0);
    lv_obj_set_style_line_rounded(trend_, true, 0);
}

void StatusPage::update(const AppState &state, int64_t monotonic_seconds) {
    if (state.link.last_packet_at > 0 && monotonic_seconds >= state.link.last_packet_at) {
        const int64_t age = monotonic_seconds - state.link.last_packet_at;
        if (age < 60) {
            lv_label_set_text(sync_age_, "just now");
        } else if (age < 3600) {
            lv_label_set_text_fmt(sync_age_, "%lldm ago", static_cast<long long>(age / 60));
        } else {
            lv_label_set_text_fmt(sync_age_, "%lldh ago", static_cast<long long>(age / 3600));
        }
    }

    const bool stale = !state.radar.valid || state.radar.stale;
    lv_label_set_text(stale_value_, stale ? "yes" : "no");
    lv_obj_set_style_text_color(stale_value_, lv_color_hex(stale ? kOrange : kBlue), 0);
    lv_label_set_text(ble_value_, state.link.ble_connected ? "connected" : "disconnected");
    lv_obj_set_style_text_color(ble_value_,
                                lv_color_hex(state.link.ble_connected ? kGreen : kMuted), 0);
    lv_label_set_text_fmt(power_value_, "%s %u%%%s",
                          state.power.usb_present ? "USB" : "BAT",
                          state.power.battery_percent,
                          state.power.charging ? " +" : "");
    lv_obj_set_style_text_color(power_value_,
                                lv_color_hex(state.power.battery_percent < 10 ? kOrange : kWhite), 0);

    const std::size_t count = std::min<std::size_t>(state.radar.trend_count, kTrendPoints);
    if (count == 0) {
        lv_obj_add_flag(trend_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(trend_, LV_OBJ_FLAG_HIDDEN);

    auto begin = state.radar.trend_iq_x10.begin();
    const auto [minimum_it, maximum_it] = std::minmax_element(begin, begin + count);
    int minimum = *minimum_it;
    int maximum = *maximum_it;
    if (minimum == maximum) {
        minimum -= 10;
        maximum += 10;
    }
    const int padding = std::max(5, (maximum - minimum) / 10);
    minimum -= padding;
    maximum += padding;

    for (std::size_t i = 0; i < count; ++i) {
        trend_points_[i].x = static_cast<lv_value_precise_t>(
            count == 1 ? 82 : (i * 165) / (count - 1));
        trend_points_[i].y = static_cast<lv_value_precise_t>(
            55 - ((state.radar.trend_iq_x10[i] - minimum) * 55) / (maximum - minimum));
    }
    lv_line_set_points(trend_, trend_points_.data(), static_cast<uint32_t>(count));
}

}  // namespace codex_island::ui
