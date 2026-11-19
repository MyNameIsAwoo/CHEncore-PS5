// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "../examples/cloneheronative/gameplay/pc_tick_clock.hpp"
#include <cassert>
#include <cstdio>
int main(){
    // Independently captured PC raw ticks in the tempo-change MIDI fixture.
    assert(rhythm::pcRawTick(2.15,2,768,192,100)==815);
    assert(rhythm::pcRawTick(3.05,2,768,192,100)==1103);
    assert(rhythm::pcRawTick(2,2,768,192,100)==768);
    assert(rhythm::pcRawTick(-1,0,0,192,120)==0);
    std::puts("PASS PC tempo-segment tick truncation and negative lead-in");
}
