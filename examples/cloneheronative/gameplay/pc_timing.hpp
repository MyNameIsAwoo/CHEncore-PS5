// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#pragma once
#include "song_pipeline.hpp"
namespace rhythm {
// BaseEngine 0x1820EFE00, neighbor helper 0x1820F35B0 and guitar
// adjustment 0x180FBB7F0. Doubles 0x18307A368/370 are 0.55238 / 42.
inline HitWindow pcPrecisionWindow(const SongChart& chart,std::size_t head) noexcept {
    if(head>=chart.notes.size()) return {};
    const auto time=chart.notes[head].time;
    const auto left=head?std::optional<Seconds>{chart.notes[head-1].time}:std::nullopt;
    const auto right=head+1<chart.notes.size()?std::optional<Seconds>{chart.notes[head+1].time}:std::nullopt;
    const auto total=neighborWindowContribution(left,time,0.0,0.0525)+
        neighborWindowContribution(time,right,0.0,0.0525);
    const auto half=((total*1000.0*0.55238+42.0)/1000.0)*0.5;
    return {half,half};
}
}
