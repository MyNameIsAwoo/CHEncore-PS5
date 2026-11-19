// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
namespace rhythm {
// Raw chart clock 0x182147460 truncates the segment-relative tick offset
// before adding the integer origin. Adding first can round just below-boundary
// times up by one tick, paying sustain points one update too soon.
inline int64_t pcRawTick(double time,double segmentTime,int64_t segmentTick,int resolution,double bpm) noexcept {
    return segmentTick+static_cast<int64_t>(((std::max(0.0,time)-segmentTime)*resolution*bpm)/60.0);
}
// Tick-to-time 0x182147170 preserves this operation order, then increments
// the double until the forward mapper round-trips to the authored tick.
inline double pcTimeAtTick(int64_t tick,double segmentTime,int64_t segmentTick,int resolution,double bpm) noexcept {
    double time=(double(tick-segmentTick)*60.0)/(bpm*resolution)+segmentTime;
    for(unsigned n=0;n<32 && std::isfinite(time) &&
        pcRawTick(time,segmentTime,segmentTick,resolution,bpm)!=tick;++n)
        time=std::nextafter(time,INFINITY);
    return time;
}
}
