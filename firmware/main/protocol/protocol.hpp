#pragma once

#include <cstdint>

#include "model/app_state.hpp"

namespace codex_island::protocol {

enum class ProcessResult {
    kAppliedUsage,
    kAppliedRadar,
    kHeartbeat,
    kDuplicate,
    kInvalid,
};

class ProtocolProcessor {
public:
    void begin_session();
    ProcessResult process_line(const char *line, int64_t monotonic_seconds,
                               AppStateStore &store);

private:
    uint32_t last_usage_sequence_ = 0;
    uint32_t last_radar_sequence_ = 0;
};

const char *result_name(ProcessResult result);

}  // namespace codex_island::protocol
