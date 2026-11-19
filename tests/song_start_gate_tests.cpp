// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/song_start_gate.hpp"
#include "gameplay/song_pipeline.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
    for(double load:{0.,3.,8.,90.}) {
        rhythm::SongStartGate gate;rhythm::AudioClock clock{rhythm::ClockConfig{}};
        gate.arm();clock.reset(-3);
        clock.update(gate.delta(load,100+load),{},!gate.waiting,false);
        assert(clock.gameplayTime()==-3);
        gate.presented(100+load);
        const double elapsed=gate.delta(load,100+load+.016);
        assert(std::abs(elapsed-.016)<1e-10);
        clock.update(elapsed,{},true,false);
        assert(std::abs(clock.gameplayTime()+2.984)<1e-10);
        clock.update(gate.delta(.02,100+load+.036),{},true,false);
        assert(std::abs(clock.gameplayTime()+2.964)<1e-10);
        gate.arm();clock.reset(-3);
        assert(gate.delta(9,200)==0 && gate.waiting);
        gate.presented(210);assert(std::abs(gate.delta(9,210.01)-.01)<1e-10);
    }
    puts("PASS cold-load/restart delays excluded from pre-roll, normal frame advancement preserved");
}
