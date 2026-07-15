#pragma once

#include <cstddef>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

namespace codex_island::transport::ble_nus {

constexpr std::size_t kMaxLineLength = 2048;

struct Line {
    char data[kMaxLineLength + 1]{};
};

enum class LinkEvent : unsigned char {
    kConnected,
    kDisconnected,
    kSubscribed,
};

esp_err_t start(const char *device_name);
bool poll_line(Line &line, TickType_t wait_ticks);
bool poll_link_event(LinkEvent &event);
bool notify_json_line(const char *json);
bool request_refresh();
bool is_connected();

}  // namespace codex_island::transport::ble_nus
