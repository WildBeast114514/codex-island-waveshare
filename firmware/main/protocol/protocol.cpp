#include "protocol/protocol.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include "cJSON.h"

namespace codex_island::protocol {
namespace {

class JsonDocument {
public:
    explicit JsonDocument(const char *line) : root_(cJSON_Parse(line)) {}
    ~JsonDocument() { cJSON_Delete(root_); }
    cJSON *get() const { return root_; }

private:
    cJSON *root_ = nullptr;
};

bool read_integer(const cJSON *parent, const char *name, int64_t minimum,
                  int64_t maximum, int64_t &value) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, name);
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
        std::floor(item->valuedouble) != item->valuedouble ||
        item->valuedouble < static_cast<double>(minimum) ||
        item->valuedouble > static_cast<double>(maximum)) {
        return false;
    }
    value = static_cast<int64_t>(item->valuedouble);
    return true;
}

bool read_array_integer(const cJSON *item, int64_t minimum, int64_t maximum,
                        int64_t &value) {
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
        std::floor(item->valuedouble) != item->valuedouble ||
        item->valuedouble < static_cast<double>(minimum) ||
        item->valuedouble > static_cast<double>(maximum)) {
        return false;
    }
    value = static_cast<int64_t>(item->valuedouble);
    return true;
}

bool read_optional_percent(const cJSON *parent, const char *name,
                           bool &available, int64_t &value) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, name);
    if (item == nullptr || cJSON_IsNull(item)) {
        available = false;
        value = 0;
        return true;
    }
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
        std::floor(item->valuedouble) != item->valuedouble ||
        item->valuedouble < 0 || item->valuedouble > 100) {
        return false;
    }
    available = true;
    value = static_cast<int64_t>(item->valuedouble);
    return true;
}

template <std::size_t Size>
bool copy_utf8(char (&destination)[Size], const char *source) {
    if (source == nullptr || source[0] == '\0') {
        return false;
    }
    std::size_t source_offset = 0;
    std::size_t output_length = 0;
    while (source[source_offset] != '\0') {
        const unsigned char lead = static_cast<unsigned char>(source[source_offset]);
        std::size_t codepoint_length = 0;
        if (lead < 0x80) {
            codepoint_length = 1;
        } else if ((lead & 0xE0) == 0xC0) {
            codepoint_length = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            codepoint_length = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            codepoint_length = 4;
        } else {
            return false;
        }
        for (std::size_t continuation = 1; continuation < codepoint_length;
             ++continuation) {
            const unsigned char byte =
                static_cast<unsigned char>(source[source_offset + continuation]);
            if (byte == 0 || (byte & 0xC0) != 0x80) {
                return false;
            }
        }
        if (output_length + codepoint_length >= Size) {
            break;
        }
        std::memcpy(destination + output_length, source + source_offset,
                    codepoint_length);
        output_length += codepoint_length;
        source_offset += codepoint_length;
    }
    destination[output_length] = '\0';
    return output_length > 0;
}

bool read_envelope(cJSON *root, const char *&kind, uint32_t &sequence,
                   int64_t &timestamp) {
    if (!cJSON_IsObject(root)) {
        return false;
    }
    int64_t version_value = 0;
    int64_t sequence_value = 0;
    if (!read_integer(root, "v", 1, 1, version_value) ||
        !read_integer(root, "seq", 1, std::numeric_limits<uint32_t>::max(),
                      sequence_value) ||
        !read_integer(root, "ts", 0, 9'007'199'254'740'991LL, timestamp)) {
        return false;
    }
    const cJSON *kind_item = cJSON_GetObjectItemCaseSensitive(root, "k");
    if (!cJSON_IsString(kind_item) || kind_item->valuestring == nullptr) {
        return false;
    }
    kind = kind_item->valuestring;
    sequence = static_cast<uint32_t>(sequence_value);
    return true;
}

bool parse_usage(cJSON *root, int64_t timestamp, UsageState &usage) {
    bool five_hour_available = false;
    bool seven_day_available = false;
    int64_t five_hour = 0;
    int64_t seven_day = 0;
    int64_t reset = 0;
    int64_t tokens = 0;
    int64_t cost = 0;
    if (!read_optional_percent(root, "p5", five_hour_available, five_hour) ||
        !read_optional_percent(root, "p7", seven_day_available, seven_day) ||
        !read_integer(root, "tok", 0, 9'007'199'254'740'991LL, tokens) ||
        !read_integer(root, "cost_c", 0, std::numeric_limits<uint32_t>::max(),
                      cost)) {
        return false;
    }
    const cJSON *reset_item = cJSON_GetObjectItemCaseSensitive(root, "reset_s");
    if (five_hour_available) {
        if (!read_integer(root, "reset_s", 0,
                          std::numeric_limits<uint32_t>::max(), reset)) {
            return false;
        }
    } else if (reset_item != nullptr && !cJSON_IsNull(reset_item) &&
               !read_integer(root, "reset_s", 0,
                             std::numeric_limits<uint32_t>::max(), reset)) {
        return false;
    }
    const cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    if (!cJSON_IsArray(daily) || cJSON_GetArraySize(daily) != kDailyPoints) {
        return false;
    }

    UsageState parsed{};
    parsed.valid = true;
    parsed.five_hour_available = five_hour_available;
    parsed.seven_day_available = seven_day_available;
    parsed.five_hour_percent = static_cast<uint8_t>(five_hour);
    parsed.seven_day_percent = static_cast<uint8_t>(seven_day);
    parsed.reset_seconds = static_cast<uint32_t>(reset);
    parsed.today_tokens = static_cast<uint64_t>(tokens);
    parsed.today_cost_cents = static_cast<uint32_t>(cost);
    parsed.updated_at = timestamp;
    for (std::size_t index = 0; index < kDailyPoints; ++index) {
        int64_t daily_value = 0;
        if (!read_array_integer(cJSON_GetArrayItem(daily, index), 0,
                                9'007'199'254'740'991LL, daily_value)) {
            return false;
        }
        parsed.daily_tokens[index] = static_cast<uint64_t>(daily_value);
    }
    usage = parsed;
    return true;
}

bool parse_radar(cJSON *root, int64_t timestamp, RadarState &radar) {
    const cJSON *stale = cJSON_GetObjectItemCaseSensitive(root, "stale");
    const cJSON *models = cJSON_GetObjectItemCaseSensitive(root, "models");
    const cJSON *trend = cJSON_GetObjectItemCaseSensitive(root, "trend");
    if (!cJSON_IsBool(stale) || !cJSON_IsArray(models) ||
        !cJSON_IsArray(trend)) {
        return false;
    }
    const int model_count = cJSON_GetArraySize(models);
    const int trend_count = cJSON_GetArraySize(trend);
    if (model_count < 1 || model_count > static_cast<int>(kMaxRadarModels) ||
        trend_count < 0 || trend_count > static_cast<int>(kTrendPoints)) {
        return false;
    }

    RadarState parsed{};
    parsed.valid = true;
    parsed.stale = cJSON_IsTrue(stale);
    parsed.updated_at = timestamp;
    parsed.count = static_cast<uint8_t>(model_count);
    for (int index = 0; index < model_count; ++index) {
        const cJSON *tuple = cJSON_GetArrayItem(models, index);
        if (!cJSON_IsArray(tuple) || cJSON_GetArraySize(tuple) != 5) {
            return false;
        }
        const cJSON *family = cJSON_GetArrayItem(tuple, 0);
        const cJSON *effort = cJSON_GetArrayItem(tuple, 1);
        int64_t iq = 0;
        int64_t passed = 0;
        int64_t total = 0;
        RadarModel &model = parsed.models[static_cast<std::size_t>(index)];
        if (!cJSON_IsString(family) || !cJSON_IsString(effort) ||
            !copy_utf8(model.family, family->valuestring) ||
            !copy_utf8(model.effort, effort->valuestring) ||
            !read_array_integer(cJSON_GetArrayItem(tuple, 2), 1, 3000, iq) ||
            !read_array_integer(cJSON_GetArrayItem(tuple, 3), 0, 255, passed) ||
            !read_array_integer(cJSON_GetArrayItem(tuple, 4), 1, 255, total) ||
            passed > total) {
            return false;
        }
        model.iq_x10 = static_cast<int16_t>(iq);
        model.passed = static_cast<uint8_t>(passed);
        model.total = static_cast<uint8_t>(total);
    }
    parsed.trend_count = static_cast<uint8_t>(trend_count);
    for (int index = 0; index < trend_count; ++index) {
        int64_t iq = 0;
        if (!read_array_integer(cJSON_GetArrayItem(trend, index), 1, 3000, iq)) {
            return false;
        }
        parsed.trend_iq_x10[static_cast<std::size_t>(index)] =
            static_cast<int16_t>(iq);
    }
    radar = parsed;
    return true;
}

}  // namespace

ProcessResult ProtocolProcessor::process_line(const char *line,
                                              int64_t monotonic_seconds,
                                              AppStateStore &store) {
    if (line == nullptr) {
        return ProcessResult::kInvalid;
    }
    JsonDocument document(line);
    cJSON *root = document.get();
    const char *kind = nullptr;
    uint32_t sequence = 0;
    int64_t timestamp = 0;
    if (!read_envelope(root, kind, sequence, timestamp)) {
        return ProcessResult::kInvalid;
    }

    if (std::strcmp(kind, "usage") == 0) {
        if (sequence <= last_usage_sequence_) {
            store.update([monotonic_seconds](AppState &state) {
                state.link.last_packet_at = monotonic_seconds;
            });
            return ProcessResult::kDuplicate;
        }
        UsageState usage{};
        if (!parse_usage(root, timestamp, usage)) {
            return ProcessResult::kInvalid;
        }
        store.update([&usage, monotonic_seconds](AppState &state) {
            state.usage = usage;
            state.link.last_packet_at = monotonic_seconds;
        });
        last_usage_sequence_ = sequence;
        return ProcessResult::kAppliedUsage;
    }
    if (std::strcmp(kind, "radar") == 0) {
        if (sequence <= last_radar_sequence_) {
            store.update([monotonic_seconds](AppState &state) {
                state.link.last_packet_at = monotonic_seconds;
            });
            return ProcessResult::kDuplicate;
        }
        RadarState radar{};
        if (!parse_radar(root, timestamp, radar)) {
            return ProcessResult::kInvalid;
        }
        store.update([&radar, monotonic_seconds](AppState &state) {
            state.radar = radar;
            state.link.last_packet_at = monotonic_seconds;
        });
        last_radar_sequence_ = sequence;
        return ProcessResult::kAppliedRadar;
    }
    return ProcessResult::kInvalid;
}

const char *result_name(ProcessResult result) {
    switch (result) {
        case ProcessResult::kAppliedUsage:
            return "usage applied";
        case ProcessResult::kAppliedRadar:
            return "radar applied";
        case ProcessResult::kDuplicate:
            return "duplicate ignored";
        case ProcessResult::kInvalid:
            return "invalid";
    }
    return "unknown";
}

}  // namespace codex_island::protocol
