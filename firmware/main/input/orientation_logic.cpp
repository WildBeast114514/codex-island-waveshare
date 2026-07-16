#include "input/orientation_logic.hpp"

#include <algorithm>
#include <cstdlib>

namespace codex_island::input {

namespace {

int64_t square(int value) {
    return static_cast<int64_t>(value) * value;
}

}  // namespace

std::optional<DisplayOrientation> OrientationFilter::update(
    int x_mg, int y_mg, int z_mg) {
    if (!smoothed_) {
        filtered_x_mg_ = x_mg;
        filtered_y_mg_ = y_mg;
        filtered_z_mg_ = z_mg;
        smoothed_ = true;
    } else {
        // A small low-pass filter removes hand tremor without making a
        // deliberate rotation feel sluggish.
        filtered_x_mg_ = (filtered_x_mg_ * 3 + x_mg) / 4;
        filtered_y_mg_ = (filtered_y_mg_ * 3 + y_mg) / 4;
        filtered_z_mg_ = (filtered_z_mg_ * 3 + z_mg) / 4;
    }

    const int64_t gravity_squared =
        square(filtered_x_mg_) + square(filtered_y_mg_) +
        square(filtered_z_mg_);
    if (gravity_squared < square(config_.minimum_gravity_mg) ||
        gravity_squared > square(config_.maximum_gravity_mg)) {
        reset_candidate();
        return std::nullopt;
    }

    const int absolute_x = std::abs(filtered_x_mg_);
    const int absolute_y = std::abs(filtered_y_mg_);
    if (std::max(absolute_x, absolute_y) < config_.minimum_planar_mg ||
        std::abs(absolute_x - absolute_y) < config_.dominance_margin_mg) {
        // Keep the current orientation while the display is face-up/down or
        // near a 45-degree boundary.
        reset_candidate();
        return std::nullopt;
    }

    DisplayOrientation observed = DisplayOrientation::k0;
    if (absolute_x > absolute_y) {
        observed = filtered_x_mg_ >= 0 ? DisplayOrientation::k0
                                      : DisplayOrientation::k180;
    } else {
        // The board layout maps QMI8658 +Y to the display's right edge.
        observed = filtered_y_mg_ >= 0 ? DisplayOrientation::k90
                                      : DisplayOrientation::k270;
    }

    if (!candidate_valid_ || observed != candidate_) {
        candidate_ = observed;
        candidate_samples_ = 1;
        candidate_valid_ = true;
        return std::nullopt;
    }

    if (candidate_samples_ < config_.stable_samples) {
        ++candidate_samples_;
    }
    if (candidate_samples_ < config_.stable_samples || candidate_ == current_) {
        return std::nullopt;
    }

    current_ = candidate_;
    return current_;
}

void OrientationFilter::reset_candidate() {
    candidate_valid_ = false;
    candidate_samples_ = 0;
}

const char *orientation_name(DisplayOrientation orientation) {
    switch (orientation) {
        case DisplayOrientation::k0:
            return "0";
        case DisplayOrientation::k90:
            return "90";
        case DisplayOrientation::k180:
            return "180";
        case DisplayOrientation::k270:
            return "270";
    }
    return "?";
}

DisplayOrientation inverse_orientation(DisplayOrientation orientation) {
    switch (orientation) {
        case DisplayOrientation::k90:
            return DisplayOrientation::k270;
        case DisplayOrientation::k270:
            return DisplayOrientation::k90;
        case DisplayOrientation::k0:
        case DisplayOrientation::k180:
            return orientation;
    }
    return DisplayOrientation::k0;
}

}  // namespace codex_island::input
