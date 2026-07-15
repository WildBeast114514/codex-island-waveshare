#pragma once

#include <atomic>
#include <array>
#include <cstdint>

#include "lvgl.h"
#include "model/app_state.hpp"
#include "ui/page_radar.hpp"
#include "ui/page_status.hpp"
#include "ui/page_usage.hpp"

namespace codex_island::ui {

class UiApp {
public:
    void begin(AppStateStore *store, uint8_t initial_page = 0,
               uint8_t initial_brightness = 35);
    void refresh(int64_t monotonic_seconds);
    void tick(int64_t monotonic_seconds);
    void set_page(uint8_t page, bool animate = true);
    void next_page();
    uint8_t current_page() const { return current_page_.load(); }
    bool take_user_activity() { return user_activity_.exchange(false); }
    bool take_brightness_request(uint8_t &brightness) {
        return status_.take_brightness_request(brightness);
    }
    void set_pixel_offset(int8_t x, int8_t y);

private:
    struct DotContext {
        UiApp *app = nullptr;
        uint8_t page = 0;
    };

    static void dot_event(lv_event_t *event);
    static void tile_event(lv_event_t *event);
    static void activity_event(lv_event_t *event);
    void create_dots(lv_obj_t *parent, uint8_t active);

    AppStateStore *store_ = nullptr;
    lv_obj_t *tileview_ = nullptr;
    std::array<lv_obj_t *, 3> tiles_{};
    std::array<DotContext, 9> dot_contexts_{};
    std::size_t next_dot_context_ = 0;
    std::atomic<uint8_t> current_page_{0};
    std::atomic<bool> user_activity_{false};
    UsagePage usage_{};
    RadarPage radar_{};
    StatusPage status_{};
};

}  // namespace codex_island::ui
