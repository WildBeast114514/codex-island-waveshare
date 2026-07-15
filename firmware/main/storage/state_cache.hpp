#pragma once

#include <cstdint>

#include "esp_err.h"
#include "model/app_state.hpp"

namespace codex_island::storage {

class StateCache {
public:
    esp_err_t begin();
    bool load(AppState &state, uint8_t &page, uint8_t &brightness);
    esp_err_t save_if_changed(const AppState &state, uint8_t page,
                              uint8_t brightness);

private:
    uint32_t last_checksum_ = 0;
    bool ready_ = false;
};

}  // namespace codex_island::storage
