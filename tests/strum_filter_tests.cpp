// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/guitar_session.hpp"
#include <cassert>
#include <cstdio>
unsigned play(double threshold,bool alternate,double second) {
    rhythm::SongChart chart{{{1,0,1},{1.01,0,1},{2,0,2}}};
    rhythm::GuitarSession session(chart);session.setStrumDebounce(threshold);
    session.update({1,false,true},1);
    session.update({1,false,false},1.001);
    session.update({1,alternate,!alternate},second);
    return session.hits();
}
int main() {
    assert(play(.005,false,1.003)==1);
    assert(play(.005,false,1.005)==2);
    assert(play(.020,false,1.010)==1);
    assert(play(.020,false,1.025)==2);
    assert(play(.050,true,1.003)==2); // PC alternating-direction bypass
    assert(play(0,false,1.003)==2);
    assert(play(-1,false,1.003)==2);
    std::puts("PASS gameplay default/custom/off strum thresholds, boundary, and alternating-direction bypass");
}
