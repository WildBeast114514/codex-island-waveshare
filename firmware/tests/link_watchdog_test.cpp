#include <cassert>

#include "transport/link_watchdog.hpp"

using codex_island::transport::link_needs_recovery;

int main() {
    assert(!link_needs_recovery(false, 100, 500, 180));
    assert(!link_needs_recovery(true, 0, 500, 180));
    assert(!link_needs_recovery(true, 100, 99, 180));
    assert(!link_needs_recovery(true, 100, 279, 180));
    assert(link_needs_recovery(true, 100, 280, 180));
    return 0;
}
