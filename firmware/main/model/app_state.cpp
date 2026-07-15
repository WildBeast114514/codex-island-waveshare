#include "model/app_state.hpp"

#include <cstdio>
#include <cstring>

namespace codex_island {

AppStateStore::AppStateStore() : mutex_(xSemaphoreCreateMutex()) {}

AppState AppStateStore::snapshot() const {
    AppState copy{};
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        copy = state_;
        xSemaphoreGive(mutex_);
    }
    return copy;
}

void AppStateStore::replace(const AppState &next) {
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        state_ = next;
        xSemaphoreGive(mutex_);
    }
}

namespace {

void set_model(RadarModel &model, const char *family, const char *effort,
               int iq_x10, int passed, int total) {
    std::snprintf(model.family, sizeof(model.family), "%s", family);
    std::snprintf(model.effort, sizeof(model.effort), "%s", effort);
    model.iq_x10 = static_cast<int16_t>(iq_x10);
    model.passed = static_cast<uint8_t>(passed);
    model.total = static_cast<uint8_t>(total);
}

}  // namespace

AppState make_static_mock_state() {
    AppState state{};
    state.usage.valid = true;
    state.usage.five_hour_available = true;
    state.usage.seven_day_available = true;
    state.usage.five_hour_percent = 42;
    state.usage.seven_day_percent = 31;
    state.usage.reset_seconds = 4980;
    state.usage.today_tokens = 486200;
    state.usage.today_cost_cents = 231;
    state.usage.daily_tokens = {15200, 83000, 47000, 92000, 66000, 101000, 486200};

    state.radar.valid = true;
    state.radar.stale = false;
    state.radar.count = 5;
    set_model(state.radar.models[0], "Sol", "max", 1200, 8, 10);
    set_model(state.radar.models[1], "Sol", "xhigh", 1050, 9, 10);
    set_model(state.radar.models[2], "Terra", "max", 1050, 8, 10);
    set_model(state.radar.models[3], "Luna", "max", 900, 6, 10);
    set_model(state.radar.models[4], "Terra", "high", 450, 4, 10);
    state.radar.trend_iq_x10 = {720, 760, 880, 840, 920, 850, 810, 900, 880, 980, 1050, 1200};
    state.radar.trend_count = 12;

    state.link.ble_connected = true;
    state.power.battery_percent = 86;
    state.power.battery_present = true;
    state.power.charging = false;
    state.power.usb_present = true;
    return state;
}

}  // namespace codex_island
