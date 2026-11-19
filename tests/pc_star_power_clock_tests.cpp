// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/pc_star_power_clock.hpp"
#include "gameplay/pc_star_power.hpp"
#include "gameplay/pc_tick_clock.hpp"
#include <cassert>
#include <cstdio>
int main(){
    rhythm::PcStarPowerClock clock;
    clock.build(192,{{0,120}},{{0,4,4}},[](long tick){return tick/384.0;});
    assert(clock.position(0)==0);assert(clock.position(16)==5760);assert(clock.timeAt(5760)==16);
    rhythm::PcStarPower power;power.reset(192);for(int i=0;i<4;++i)power.awardPhrase();
    assert(power.activate(clock.position(0)/192));
    assert(!power.advanceTicks(static_cast<long>(clock.position(15))));assert(power.active() && power.remainingTicks()==360);
    assert(power.advanceTicks(static_cast<long>(clock.position(16))));assert(!power.active() && power.remainingTicks()==0 && power.fill()==0);
    clock.build(192,{{0,120}},{{0,6,4}},[](long tick){return tick/384.0;});
    assert(clock.position(24)==5760);assert(clock.timeAt(5760)==24);
    clock.build(192,{{0,120}},{{0,6,8}},[](long tick){return tick/384.0;});
    assert(clock.position(12)==5760);assert(clock.timeAt(5760)==12);
    clock.build(192,{{0,120}},{{0,4,4},{1536,6,4}},[](long tick){return tick/384.0;});
    for(double t:{0.,3.99,4.,4.01,10.,20.})assert(std::abs(clock.timeAt(clock.position(t))-t)<1e-12);
    power.reset(192);power.awardPhrase();power.awardPhrase();
    assert(power.activatePosition(12.5,1.25));
    assert(!power.advanceTicks(2892,false) && power.active() && power.remainingTicks()==0);
    assert(power.expirationTick()==2892 && power.activationTick()==12 && power.activationTime()==1.25);
    assert(!power.advanceTicks(2893,false) && power.expirationTick()==2892);
    assert(power.advanceTicks(2893) && !power.active());
    clock.build(192,{{0,120}},{{0,4,4}},[](long tick){return tick/384.0;});
    assert(clock.afterTicks(1.25,2880)==9.25);
    for(double bpm:{120.,162.60162601626016,162.16216216216216})
        for(int64_t tick=0;tick<200000;tick+=17) {
            const auto time=rhythm::pcTimeAtTick(tick,0,0,480,bpm);
            assert(rhythm::pcRawTick(time,0,0,480,bpm)==tick);
        }
    puts("PASS PC eight-measure SP timing, signature changes, inverse mapping and empty-on-expiry");
}
