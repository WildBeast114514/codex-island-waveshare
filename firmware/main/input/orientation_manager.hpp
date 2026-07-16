#pragma once

#include "esp_err.h"
#include "input/orientation_logic.hpp"
#include "qmi8658.h"

namespace codex_island::input {

class OrientationManager {
public:
    esp_err_t begin();
    bool poll(DisplayOrientation &orientation);

private:
    qmi8658_dev_t sensor_{};
    OrientationFilter filter_{};
    bool ready_ = false;
};

}  // namespace codex_island::input
