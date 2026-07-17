#include <cassert>

#include "power/display_link_policy.hpp"

using codex_island::power::bridge_link_should_sleep_display;

int main() {
    // Cold boot gets a full connection/traffic grace period.
    assert(!bridge_link_should_sleep_display(false, 0, 10, 10, 159));
    assert(bridge_link_should_sleep_display(false, 0, 10, 10, 160));

    // A proper disconnect after successful traffic turns the screen off soon.
    assert(!bridge_link_should_sleep_display(false, 100, 120, 50, 134));
    assert(bridge_link_should_sleep_display(false, 100, 120, 50, 135));

    // A half-open BLE link is detected by missing valid Bridge traffic.
    assert(!bridge_link_should_sleep_display(true, 100, 90, 50, 249));
    assert(bridge_link_should_sleep_display(true, 100, 90, 50, 250));

    // Reconnection starts a fresh subscription grace period.
    assert(!bridge_link_should_sleep_display(true, 100, 300, 50, 449));
    assert(bridge_link_should_sleep_display(true, 100, 300, 50, 450));

    // Touch/button wake remains visible for a useful interval while offline.
    assert(!bridge_link_should_sleep_display(false, 100, 120, 200, 229));
    assert(bridge_link_should_sleep_display(false, 100, 120, 200, 230));

    // Future timestamps and invalid policy inputs fail open, keeping UI visible.
    assert(!bridge_link_should_sleep_display(true, 500, 500, 0, 400));
    assert(!bridge_link_should_sleep_display(true, 100, 100, 0, 500,
                                             0, 150, 30));
    return 0;
}
