#include <cassert>

#include "input/orientation_logic.hpp"

using codex_island::input::DisplayOrientation;
using codex_island::input::OrientationConfig;
using codex_island::input::OrientationFilter;
using codex_island::input::inverse_orientation;

namespace {

void feed(OrientationFilter &filter, int x, int y, int z, int count,
          DisplayOrientation *changed = nullptr) {
    for (int i = 0; i < count; ++i) {
        const auto result = filter.update(x, y, z);
        if (result.has_value() && changed != nullptr) {
            *changed = *result;
        }
    }
}

}  // namespace

int main() {
    assert(inverse_orientation(DisplayOrientation::k0) ==
           DisplayOrientation::k0);
    assert(inverse_orientation(DisplayOrientation::k90) ==
           DisplayOrientation::k270);
    assert(inverse_orientation(DisplayOrientation::k180) ==
           DisplayOrientation::k180);
    assert(inverse_orientation(DisplayOrientation::k270) ==
           DisplayOrientation::k90);

    OrientationConfig config{};
    config.stable_samples = 4;

    {
        OrientationFilter filter(config);
        feed(filter, 0, 0, 1000, 20);
        assert(filter.current() == DisplayOrientation::k0);
    }

    {
        OrientationFilter filter(config);
        feed(filter, 690, 710, 0, 20);
        assert(filter.current() == DisplayOrientation::k0);
    }

    {
        OrientationFilter filter(config);
        feed(filter, 0, 1000, 0, 3);
        assert(filter.current() == DisplayOrientation::k0);
        DisplayOrientation changed = DisplayOrientation::k0;
        feed(filter, 0, 1000, 0, 2, &changed);
        assert(changed == DisplayOrientation::k90);
        assert(filter.current() == DisplayOrientation::k90);
    }

    {
        OrientationFilter filter(config);
        DisplayOrientation changed = DisplayOrientation::k0;
        feed(filter, -1000, 0, 0, 5, &changed);
        assert(changed == DisplayOrientation::k180);
    }

    {
        OrientationFilter filter(config);
        DisplayOrientation changed = DisplayOrientation::k0;
        feed(filter, 0, -1000, 0, 5, &changed);
        assert(changed == DisplayOrientation::k270);
    }

    {
        OrientationFilter filter(config);
        feed(filter, 1900, 0, 0, 20);
        assert(filter.current() == DisplayOrientation::k0);
    }
}
