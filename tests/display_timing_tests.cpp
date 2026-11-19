// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/display_timing.hpp"
#include <cassert>
#include <initializer_list>
#include <cstdio>
int main() {
    rhythm::DisplayTiming display;
    assert(!display.supports120() && !display.choiceEnabled(2));
    assert(display.effectiveChoice(2)==1 && display.targetMilliHz(2)==59940);
    for(unsigned hz:{60000u,59940u,119880u,120000u}) {
        display.refreshMilliHz=hz;
        assert(display.choiceEnabled(0) && display.choiceEnabled(1));
        assert(display.choiceEnabled(2)==(hz>=110000));
        assert(display.targetMilliHz(0)==hz);
        assert(display.targetMilliHz(1)==(hz>=110000?hz/2:hz));
        assert(display.targetMilliHz(2)==hz);
    }
    display.refreshMilliHz=59940;
    assert(display.effectiveChoice(2)==1);
    display.refreshMilliHz=119880;
    assert(display.effectiveChoice(2)==2 && display.flipRate(1)==1);
    assert(!display.choiceEnabled(-1) && !display.choiceEnabled(3));
    puts("PASS display fallback, saved preference, 120 lockout and fractional vblank pacing");
}
