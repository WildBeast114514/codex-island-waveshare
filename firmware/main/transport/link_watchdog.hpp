#pragma once

#include <cstdint>

namespace codex_island::transport {

constexpr int64_t kBleLinkTimeoutSeconds = 180;

inline bool link_needs_recovery(
    bool connected, int64_t last_activity_seconds, int64_t now_seconds,
    int64_t timeout_seconds = kBleLinkTimeoutSeconds) {
    return connected && last_activity_seconds > 0 && timeout_seconds > 0 &&
           now_seconds >= last_activity_seconds &&
           now_seconds - last_activity_seconds >= timeout_seconds;
}

}  // namespace codex_island::transport
