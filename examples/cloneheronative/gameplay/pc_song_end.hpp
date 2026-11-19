// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
namespace rhythm {
// GameManager preparation 0x1802ACE60 takes the longer audio/global-chart
// endpoint, caps it at an authored end event, then adds one second. Update
// 0x18028E370 compares the shared song clock to that deadline.
inline double pcSongFinishTime(double chartEnd,double audioEnd,double authoredEnd=-1) noexcept {
    if(!std::isfinite(chartEnd) || chartEnd<0)chartEnd=0;
    if(!std::isfinite(audioEnd) || audioEnd<0)audioEnd=0;
    double end=std::max(chartEnd,audioEnd);
    if(std::isfinite(authoredEnd) && authoredEnd>=0)end=std::min(end,authoredEnd);
    return end+1.0;
}
}
