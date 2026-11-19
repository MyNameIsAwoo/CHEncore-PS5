// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/results_stats.hpp"
#include <cassert>
#include <cstdio>
int main(){
 rhythm::ResultsStats s;
 s.award(50,30,true,false,true);
 s.award(1,30,true,true);
 assert(s.notes==50&&s.sustains==1&&s.combo==153&&s.starPower==204);
 assert(s.positiveSqueezes==1&&s.squeezeScore==200);
 s.award(50,20,false,false,false,true);
 assert(s.negativeSqueezes==1&&s.squeezeScore==100);
 s.lastSpSample=5;s.advanceSp(5.5,true,6);s.advanceSp(5.5,true,6);s.advanceSp(6.2,true,6);
 assert(s.spSeconds==1&&s.lastSpEnd==6);
 s.advanceSp(7,false,6);assert(s.spSeconds==1);
 std::puts("PASS: PC head/sustain score breakdown, squeeze ledger and bounded SP duration without duplicate time");
}
