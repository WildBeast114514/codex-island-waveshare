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

lv_obj_t *make_vertical_divider(lv_obj_t *parent, int height, int x, int y) {
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, 1, height);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_style_bg_color(line, lv_color_hex(kDivider), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    return line;
}

}  // namespace

void UsagePage::create(lv_obj_t *parent) {
    style_black_surface(parent);

    link_ = make_label(parent, LV_SYMBOL_WIFI, &lv_font_montserrat_20, kWhite);
    lv_obj_set_pos(link_, 143, 44);
    lv_obj_t *title = make_label(parent, "CODEX", &lv_font_montserrat_22, kWhite);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 39);
    battery_.create(parent, 326, 47);

    arc_.create(parent, 113, 70, 240);

    percent_ = make_label(parent, "42", &lv_font_montserrat_48, kWhite);
    lv_obj_align(percent_, LV_ALIGN_TOP_MID, -8, 129);
    lv_obj_t *percent_sign = make_label(parent, "%", &lv_font_montserrat_32, kWhite);
    lv_obj_set_pos(percent_sign, 276, 158);
    lv_obj_t *window = make_label(parent, "5h window", &lv_font_montserrat_18, kMuted);
    lv_obj_align(window, LV_ALIGN_TOP_MID, 0, 201);
    make_divider(parent, 130, 168, 229);
    reset_ = make_label(parent, "reset in 1h 23m", &lv_font_montserrat_16, kOrange);
    lv_obj_align(reset_, LV_ALIGN_TOP_MID, 0, 237);

    waiting_ = make_label(parent, "Waiting for Mac", &lv_font_montserrat_18, kMuted);
    lv_obj_align(waiting_, LV_ALIGN_TOP_MID, 0, 169);
    lv_obj_add_flag(waiting_, LV_OBJ_FLAG_HIDDEN);

    make_divider(parent, 282, 92, 279);
    weekly_ = make_label(parent, "7 DAY 31%", &lv_font_montserrat_22, kWhite);
    lv_obj_align(weekly_, LV_ALIGN_TOP_MID, 0, 287);

    constexpr char days[] = {'M', 'T', 'W', 'T', 'F', 'S', 'S'};
    for (std::size_t i = 0; i < kDailyPoints; ++i) {
        const int x = 147 + static_cast<int>(i) * 25;
        bars_[i] = lv_obj_create(parent);
        lv_obj_set_size(bars_[i], 11, 16);
        lv_obj_set_pos(bars_[i], x, 350);
        lv_obj_set_style_radius(bars_[i], 5, 0);
        lv_obj_set_style_border_width(bars_[i], 0, 0);
        lv_obj_set_style_bg_color(bars_[i], lv_color_hex(kBlue), 0);
        lv_obj_set_style_bg_grad_color(bars_[i], lv_color_hex(kMagenta), 0);
        lv_obj_set_style_bg_grad_dir(bars_[i], LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(bars_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(bars_[i], 0, 0);

        char day[2] = {days[i], '\0'};
        lv_obj_t *label = make_label(parent, day, &lv_font_montserrat_12, kMuted);
        lv_obj_set_pos(label, x, 374);
    }

    make_divider(parent, 282, 92, 398);
    create_badge(parent, 149, 408, 32, kBlue, "=", &lv_font_montserrat_18);
    lv_obj_t *today = make_label(parent, "Today", &lv_font_montserrat_14, kWhite);
    lv_obj_set_pos(today, 190, 405);
    tokens_ = make_label(parent, "486K tok", &lv_font_montserrat_20, kWhite);
    lv_obj_set_pos(tokens_, 190, 422);

    make_vertical_divider(parent, 37, 290, 405);
    create_badge(parent, 306, 408, 32, kGreen, "$", &lv_font_montserrat_20);
    cost_ = make_label(parent, "$2.31", &lv_font_montserrat_20, kWhite);
    lv_obj_set_pos(cost_, 345, 416);
}

void UsagePage::update(const AppState &state) {
    battery_.update(state.power.battery_percent, state.power.charging);
    lv_obj_set_style_text_color(link_, lv_color_hex(state.link.ble_connected ? kWhite : kMuted), 0);

    if (!state.usage.valid) {
        lv_obj_clear_flag(waiting_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(percent_, LV_OBJ_FLAG_HIDDEN);
        arc_.set_value(0);
        return;
    }

    lv_obj_add_flag(waiting_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(percent_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(percent_, "%u", state.usage.five_hour_percent);
    arc_.set_value(state.usage.five_hour_percent);

    lv_label_set_text_fmt(weekly_, "7 DAY %u%%", state.usage.seven_day_percent);
    const uint32_t hours = state.usage.reset_seconds / 3600;
    const uint32_t minutes = (state.usage.reset_seconds % 3600) / 60;
    if (hours > 0) {
        lv_label_set_text_fmt(reset_, "reset in %luh %02lum",
                              static_cast<unsigned long>(hours), static_cast<unsigned long>(minutes));
    } else {
        lv_label_set_text_fmt(reset_, "reset in %lum", static_cast<unsigned long>(minutes));
    }

    const uint64_t maximum = *std::max_element(state.usage.daily_tokens.begin(),
                                                state.usage.daily_tokens.end());
    for (std::size_t i = 0; i < kDailyPoints; ++i) {
        const int height = maximum == 0 ? 8 :
            8 + static_cast<int>((state.usage.daily_tokens[i] * 40) / maximum);
        lv_obj_set_height(bars_[i], height);
        lv_obj_set_y(bars_[i], 369 - height);
    }

    char token_text[24]{};
    format_tokens(token_text, sizeof(token_text), state.usage.today_tokens);
    lv_label_set_text(tokens_, token_text);
    lv_label_set_text_fmt(cost_, "$%lu.%02lu",
                          static_cast<unsigned long>(state.usage.today_cost_cents / 100),
                          static_cast<unsigned long>(state.usage.today_cost_cents % 100));
}

}  // namespace codex_island::ui
