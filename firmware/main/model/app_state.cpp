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

void set_distributed_row(DistributedRadarRow &row, const char *model,
                         const char *effort, int iq, int passed, int total,
                         bool aggregate) {
    std::snprintf(row.model, sizeof(row.model), "%s", model);
    std::snprintf(row.effort, sizeof(row.effort), "%s", effort);
    row.iq = static_cast<uint8_t>(iq);
    row.passed = static_cast<uint16_t>(passed);
    row.total = static_cast<uint16_t>(total);
    row.has_data = total > 0;
    row.aggregate = aggregate;
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
    state.radar.updated_at = 1'700'000'000;
    std::snprintf(state.radar.updated_label, sizeof(state.radar.updated_label),
                  "11-15 06:13");
    state.radar.count = 5;
    set_model(state.radar.models[0], "Sol", "max", 1200, 8, 10);
    set_model(state.radar.models[1], "Sol", "xhigh", 1050, 9, 10);
    set_model(state.radar.models[2], "Terra", "max", 1050, 8, 10);
    set_model(state.radar.models[3], "Luna", "max", 900, 6, 10);
    set_model(state.radar.models[4], "Terra", "high", 450, 4, 10);
    state.radar.trend_iq_x10 = {720, 760, 880, 840, 920, 850, 810, 900, 880, 980, 1050, 1200};
    state.radar.trend_count = 12;

    state.distributed_radar.valid = true;
    state.distributed_radar.stale = false;
    state.distributed_radar.updated_at = 1'700'000'000;
    std::snprintf(state.distributed_radar.updated_label,
                  sizeof(state.distributed_radar.updated_label),
                  "11-15 06:13");
    state.distributed_radar.count = 8;
    set_distributed_row(state.distributed_radar.rows[0], "Sol", "", 89, 415,
                        700, true);
    set_distributed_row(state.distributed_radar.rows[1], "Sol", "low", 78, 48,
                        92, false);
    set_distributed_row(state.distributed_radar.rows[2], "Sol", "max", 101, 88,
                        131, false);
    set_distributed_row(state.distributed_radar.rows[3], "Terra", "", 76, 189,
                        373, true);
    set_distributed_row(state.distributed_radar.rows[4], "Terra", "low", 43, 20,
                        69, false);
    set_distributed_row(state.distributed_radar.rows[5], "Terra", "ultra", 114,
                        50, 66, false);
    set_distributed_row(state.distributed_radar.rows[6], "Luna", "", 52, 130,
                        372, true);
    set_distributed_row(state.distributed_radar.rows[7], "Luna", "max", 97, 51,
                        79, false);

    state.link.ble_connected = true;
    state.power.battery_percent = 86;
    state.power.battery_present = true;
    state.power.charging = false;
    state.power.usb_present = true;
    state.pet.valid = true;
    state.pet.activity = PetActivity::kRunning;
    state.pet.active_tasks = 1;
    state.pet.updated_at = 1'700'000'000;
    return state;
}

}  // namespace codex_island
