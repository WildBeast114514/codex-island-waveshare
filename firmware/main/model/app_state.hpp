#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace codex_island {

constexpr std::size_t kMaxRadarModels = 24;
constexpr std::size_t kDailyPoints = 7;
constexpr std::size_t kTrendPoints = 12;

struct UsageState {
    bool valid = false;
    bool five_hour_available = false;
    bool seven_day_available = false;
    uint8_t five_hour_percent = 0;
    uint8_t seven_day_percent = 0;
    uint32_t reset_seconds = 0;
    uint64_t today_tokens = 0;
    uint32_t today_cost_cents = 0;
    std::array<uint64_t, kDailyPoints> daily_tokens{};
    int64_t updated_at = 0;
};

struct RadarModel {
    char family[32]{};
    char effort[32]{};
    int16_t iq_x10 = 0;
    uint8_t passed = 0;
    uint8_t total = 0;
};

struct RadarState {
    bool valid = false;
    bool stale = true;
    int64_t updated_at = 0;
    char updated_label[20]{};
    uint8_t count = 0;
    std::array<RadarModel, kMaxRadarModels> models{};
    std::array<int16_t, kTrendPoints> trend_iq_x10{};
    uint8_t trend_count = 0;
};

struct LinkState {
    bool ble_connected = false;
    int64_t last_packet_at = 0;
};

struct PowerState {
    uint8_t battery_percent = 0;
    bool battery_present = false;
    bool charging = false;
    bool usb_present = false;
};

struct AppState {
    UsageState usage;
    RadarState radar;
    LinkState link;
    PowerState power;
};

class AppStateStore {
public:
    AppStateStore();
    AppState snapshot() const;
    void replace(const AppState &next);

    template <typename Callback>
    void update(Callback &&callback) {
        if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            callback(state_);
            xSemaphoreGive(mutex_);
        }
    }

private:
    mutable SemaphoreHandle_t mutex_ = nullptr;
    AppState state_{};
};

AppState make_static_mock_state();

}  // namespace codex_island
