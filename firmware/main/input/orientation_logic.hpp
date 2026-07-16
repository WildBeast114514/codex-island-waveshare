#pragma once

#include <cstdint>
#include <optional>

namespace codex_island::input {

enum class DisplayOrientation : uint8_t {
    k0 = 0,
    k90 = 1,
    k180 = 2,
    k270 = 3,
};

struct OrientationConfig {
    int minimum_planar_mg = 550;
    int dominance_margin_mg = 180;
    int minimum_gravity_mg = 650;
    int maximum_gravity_mg = 1350;
    uint8_t stable_samples = 8;
};

class OrientationFilter {
public:
    explicit OrientationFilter(
        OrientationConfig config = {},
        DisplayOrientation initial = DisplayOrientation::k0)
        : config_(config), current_(initial) {}

    std::optional<DisplayOrientation> update(int x_mg, int y_mg, int z_mg);
    DisplayOrientation current() const { return current_; }

private:
    void reset_candidate();

    OrientationConfig config_{};
    DisplayOrientation current_ = DisplayOrientation::k0;
    DisplayOrientation candidate_ = DisplayOrientation::k0;
    uint8_t candidate_samples_ = 0;
    bool candidate_valid_ = false;
    bool smoothed_ = false;
    int filtered_x_mg_ = 0;
    int filtered_y_mg_ = 0;
    int filtered_z_mg_ = 0;
};

const char *orientation_name(DisplayOrientation orientation);
DisplayOrientation inverse_orientation(DisplayOrientation orientation);

}  // namespace codex_island::input
