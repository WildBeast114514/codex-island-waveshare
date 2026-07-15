#include "ui/page_usage.hpp"

#include <algorithm>
#include <cstdio>

#include "ui/ui_theme.hpp"

namespace codex_island::ui {
namespace {

void format_tokens(char *out, std::size_t size, uint64_t tokens) {
    if (tokens >= 1'000'000) {
        std::snprintf(out, size, "%.1fM tok", tokens / 1'000'000.0);
    } else if (tokens >= 1'000) {
        std::snprintf(out, size, "%lluK tok",
                      static_cast<unsigned long long>((tokens + 500) / 1'000));
    } else {
        std::snprintf(out, size, "%llu tok", static_cast<unsigned long long>(tokens));
    }
}

lv_obj_t *make_centered_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, uint32_t color,
                              int x, int y, int width) {
    lv_obj_t *label = make_label(parent, text, font, color);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

}  // namespace

void UsagePage::create(lv_obj_t *parent) {
    style_black_surface(parent);

    link_ = make_label(parent, LV_SYMBOL_WIFI, &lv_font_montserrat_20, kWhite);
    lv_obj_set_pos(link_, 143, 44);
    lv_obj_t *title = make_label(parent, "CODEX", &lv_font_montserrat_22, kWhite);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 39);
    battery_.create(parent, 326, 47);

    five_hour_arc_.create(parent, 55, 78, 144, kBlue, kCyan, kGreen);
    seven_day_arc_.create(parent, 267, 78, 144, kPurple, kMagenta, kOrange);

    five_hour_percent_ = make_centered_label(parent, "42%", &lv_font_montserrat_32,
                                              kWhite, 63, 119, 128);
    five_hour_label_ = make_centered_label(parent, "5H", &lv_font_montserrat_16,
                                            kMuted, 63, 158, 128);
    five_hour_detail_ = make_centered_label(parent, "reset in 1h 23m",
                                             &lv_font_montserrat_12, kOrange,
                                             58, 183, 138);

    seven_day_percent_ = make_centered_label(parent, "31%", &lv_font_montserrat_32,
                                              kWhite, 275, 119, 128);
    seven_day_label_ = make_centered_label(parent, "7 DAY", &lv_font_montserrat_16,
                                            kMuted, 275, 158, 128);
    seven_day_detail_ = make_centered_label(parent, "weekly usage",
                                             &lv_font_montserrat_12, kMagenta,
                                             270, 183, 138);

    waiting_ = make_centered_label(parent, "Waiting for Mac",
                                    &lv_font_montserrat_18, kMuted,
                                    133, 139, 200);
    lv_obj_add_flag(waiting_, LV_OBJ_FLAG_HIDDEN);

    make_divider(parent, 282, 92, 239);
    lv_obj_t *activity = make_centered_label(parent, "7-DAY ACTIVITY",
                                              &lv_font_montserrat_14, kMuted,
                                              133, 248, 200);
    (void)activity;

    constexpr char days[] = {'M', 'T', 'W', 'T', 'F', 'S', 'S'};
    for (std::size_t i = 0; i < kDailyPoints; ++i) {
        const int x = 137 + static_cast<int>(i) * 29;
        bars_[i] = lv_obj_create(parent);
        lv_obj_set_size(bars_[i], 13, 16);
        lv_obj_set_pos(bars_[i], x, 310);
        lv_obj_set_style_radius(bars_[i], 5, 0);
        lv_obj_set_style_border_width(bars_[i], 0, 0);
        lv_obj_set_style_bg_color(bars_[i], lv_color_hex(kBlue), 0);
        lv_obj_set_style_bg_grad_color(bars_[i], lv_color_hex(kMagenta), 0);
        lv_obj_set_style_bg_grad_dir(bars_[i], LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(bars_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(bars_[i], 0, 0);

        char day[2] = {days[i], '\0'};
        lv_obj_t *label = make_label(parent, day, &lv_font_montserrat_12, kMuted);
        lv_obj_set_pos(label, x + 1, 333);
    }

    make_divider(parent, 282, 92, 354);
    create_badge(parent, 105, 367, 30, kBlue, "=", &lv_font_montserrat_16);
    lv_obj_t *today = make_label(parent, "Today", &lv_font_montserrat_14, kWhite);
    lv_obj_set_pos(today, 143, 362);
    tokens_ = make_label(parent, "486K tok", &lv_font_montserrat_18, kWhite);
    lv_obj_set_pos(tokens_, 143, 380);

    create_badge(parent, 271, 367, 30, kGreen, "$", &lv_font_montserrat_18);
    cost_ = make_label(parent, "$2.31", &lv_font_montserrat_18, kWhite);
    lv_obj_set_pos(cost_, 309, 373);
}

void UsagePage::update(const AppState &state) {
    battery_.update(state.power.battery_percent, state.power.charging);
    lv_obj_set_style_text_color(link_, lv_color_hex(state.link.ble_connected ? kWhite : kMuted), 0);

    if (!state.usage.valid) {
        lv_obj_clear_flag(waiting_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(five_hour_percent_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(five_hour_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(five_hour_detail_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(seven_day_percent_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(seven_day_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(seven_day_detail_, LV_OBJ_FLAG_HIDDEN);
        five_hour_arc_.set_value(0);
        seven_day_arc_.set_value(0);
        lv_label_set_text(tokens_, "--");
        lv_label_set_text(cost_, "--");
        return;
    }

    lv_obj_add_flag(waiting_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(five_hour_percent_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(five_hour_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(five_hour_detail_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(seven_day_percent_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(seven_day_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(seven_day_detail_, LV_OBJ_FLAG_HIDDEN);

    if (state.usage.five_hour_available) {
        lv_label_set_text_fmt(five_hour_percent_, "%u%%",
                              state.usage.five_hour_percent);
        lv_label_set_text(five_hour_detail_, "");
        const uint32_t hours = state.usage.reset_seconds / 3600;
        const uint32_t minutes = (state.usage.reset_seconds % 3600) / 60;
        if (hours > 0) {
            lv_label_set_text_fmt(five_hour_detail_, "reset in %luh %02lum",
                                  static_cast<unsigned long>(hours),
                                  static_cast<unsigned long>(minutes));
        } else {
            lv_label_set_text_fmt(five_hour_detail_, "reset in %lum",
                                  static_cast<unsigned long>(minutes));
        }
        five_hour_arc_.set_value(state.usage.five_hour_percent);
    } else {
        lv_label_set_text(five_hour_percent_, "N/A");
        lv_label_set_text(five_hour_detail_, "limit not reported");
        five_hour_arc_.set_value(0);
    }

    if (state.usage.seven_day_available) {
        lv_label_set_text_fmt(seven_day_percent_, "%u%%",
                              state.usage.seven_day_percent);
        lv_label_set_text(seven_day_detail_, "weekly usage");
        seven_day_arc_.set_value(state.usage.seven_day_percent);
    } else {
        lv_label_set_text(seven_day_percent_, "N/A");
        lv_label_set_text(seven_day_detail_, "limit not reported");
        seven_day_arc_.set_value(0);
    }

    const uint64_t maximum = *std::max_element(state.usage.daily_tokens.begin(),
                                                state.usage.daily_tokens.end());
    for (std::size_t i = 0; i < kDailyPoints; ++i) {
        const int height = maximum == 0 ? 8 :
            8 + static_cast<int>((state.usage.daily_tokens[i] * 40) / maximum);
        lv_obj_set_height(bars_[i], height);
        lv_obj_set_y(bars_[i], 327 - height);
    }

    char token_text[24]{};
    format_tokens(token_text, sizeof(token_text), state.usage.today_tokens);
    lv_label_set_text(tokens_, token_text);
    lv_label_set_text_fmt(cost_, "$%lu.%02lu",
                          static_cast<unsigned long>(state.usage.today_cost_cents / 100),
                          static_cast<unsigned long>(state.usage.today_cost_cents % 100));
}

}  // namespace codex_island::ui
