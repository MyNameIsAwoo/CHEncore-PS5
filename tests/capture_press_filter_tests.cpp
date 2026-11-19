// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/controller_profile.hpp"
#include <cassert>
#include <cstdio>
int main() {
    using namespace rhythm::controls;
    CapturePressFilter filter;
    Locator green;green.byte=0;green.mask=2;
    Locator red=green;red.mask=4;
    Locator up;up.kind=Kind::Hat;up.byte=2;up.value=0;
    Locator down=up;down.value=4;
    assert(filter.accept(green,0));
    assert(!filter.accept(green,.01));
    assert(filter.accept(red,.02));
    assert(!filter.accept(green,.03)); // another fret cannot reset green's bounce history
    assert(filter.accept(up,.04));
    assert(filter.accept(down,.041)); // distinct strum directions are distinct capture inputs
    assert(!filter.accept(up,.05));
    assert(filter.accept(green,.075));
    CapturePressFilter other;assert(other.accept(green,.01));
    std::puts("PASS raw remap input bounce filtering, independent inputs and players");
}
