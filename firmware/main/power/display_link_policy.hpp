#pragma once

#include <algorithm>
#include <cstdint>

namespace codex_island::power {

constexpr int64_t kBridgeDisconnectScreenOffSeconds = 15;
constexpr int64_t kBridgeTrafficScreenOffSeconds = 150;
constexpr int64_t kBridgeUserWakeGraceSeconds = 30;

inline bool bridge_link_should_sleep_display(
    bool connected, int64_t last_valid_activity_seconds,
    int64_t link_state_changed_seconds, int64_t last_user_activity_seconds,
    int64_t now_seconds,
    int64_t disconnect_delay_seconds = kBridgeDisconnectScreenOffSeconds,
    int64_t traffic_timeout_seconds = kBridgeTrafficScreenOffSeconds,
    int64_t user_wake_grace_seconds = kBridgeUserWakeGraceSeconds) {
    if (now_seconds < 0 || disconnect_delay_seconds <= 0 ||
        traffic_timeout_seconds <= 0 || user_wake_grace_seconds < 0) {
        return false;
    }
    if (last_user_activity_seconds >= 0 &&
        now_seconds >= last_user_activity_seconds &&
        now_seconds - last_user_activity_seconds < user_wake_grace_seconds) {
        return false;
    }

    const int64_t link_reference = std::max<int64_t>(
        0, connected ? std::max(last_valid_activity_seconds,
                                link_state_changed_seconds)
                     : link_state_changed_seconds);
    if (now_seconds < link_reference) {
        return false;
    }

    // A disconnected device that has exchanged valid application traffic can
    // react quickly to a real CoreBluetooth disconnect. At cold boot, or
    // while a new connection is still subscribing, use the longer traffic
    // timeout so normal startup cannot blank the display prematurely.
    const bool previously_synchronized = last_valid_activity_seconds > 0;
    const int64_t timeout =
        !connected && previously_synchronized ? disconnect_delay_seconds
                                              : traffic_timeout_seconds;
    return now_seconds - link_reference >= timeout;
}

}  // namespace codex_island::power
