// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/pc_tick_clock.hpp"
#include "gameplay/pc_scoring.hpp"
#include <cassert>
#include <cstdio>
int main() {
 const auto tick=[](double t){return rhythm::pcRawTick(t,0,0,192,120);};
 const auto before=[&](double t){return tick(std::nextafter(t,-INFINITY));};
 // PC configured four-player reference: miss at3.33, floor tick1278.
 // Its sustain point at tick1278 occurred BEFORE the multiplier break.
 assert(tick(3.33)==1278 && before(3.33)==1278);
 const auto paid=17u;
 const auto due=rhythm::pc_scoring::sustainPoints(192,before(3.33)-1152,192);
 assert(due==18);
 const auto pre=(due-paid)*rhythm::pc_scoring::multiplier(18,false);
 const auto post=(rhythm::pc_scoring::sustainPoints(192,tick(200./60)-1152,192)-due)*rhythm::pc_scoring::multiplier(0,false);
 assert(pre+post==2);
 // Exactly on the integer tick, the point belongs to post-input settlement.
 const double exact=1278./384;
 assert(tick(exact)==1278 && before(exact)==1277);
 // A non-tick-aligned hit/miss may not shift the previously elapsed tick away.
 for(double t:{3.33,3.330001,3.3333333333333335})assert(before(t)==tick(t));
 std::puts("PASS fractional/exact input boundaries: sustain points use the correct side of a multiplier change");
}
