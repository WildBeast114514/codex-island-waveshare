#include <cassert>

#include "input/button_logic.hpp"

using codex_island::input::ButtonStateMachine;

int main() {
    ButtonStateMachine single;
    single.update(true, 10);
    single.update(true, 50);
    single.update(false, 100);
    single.update(false, 140);
    assert(single.update(false, 550).short_press);

    ButtonStateMachine double_press;
    double_press.update(true, 10);
    double_press.update(true, 50);
    double_press.update(false, 80);
    double_press.update(false, 120);
    double_press.update(true, 200);
    double_press.update(true, 240);
    double_press.update(false, 280);
    assert(double_press.update(false, 320).double_press);

    ButtonStateMachine long_press;
    long_press.update(true, 10);
    long_press.update(true, 50);
    assert(long_press.update(true, 1250).long_press);
    assert(!long_press.update(true, 1400).long_press);
    return 0;
}
