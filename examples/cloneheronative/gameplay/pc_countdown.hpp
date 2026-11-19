// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include "song_pipeline.hpp"
namespace rhythm {
struct PcCountdown {Seconds start{},duration{};int endMeasure{};};
// PC converter 0x1820BAF50: countdowns span gaps strictly longer than 10s,
// excluding sustained notes. They do not replace the song clock's lead-in.
// Special freestyle-region splits are supplied by the chart converter separately.
inline std::vector<PcCountdown> pcCountdownRanges(const SongChart& chart,const std::vector<Seconds>& measures) {
    std::vector<PcCountdown> out;Seconds previousEnd=0;
    for(const auto& note:chart.notes) {
        if(note.time-previousEnd>10.0) {
            const auto next=std::upper_bound(measures.begin(),measures.end(),note.time);
            out.push_back({previousEnd,note.time-previousEnd,std::max(0,int(next-measures.begin())-1)});
        }
        previousEnd=std::max(previousEnd,note.time+note.duration);
    }
    return out;
}
// Countdown.Update 0x180123730: last three seconds fade to zero at two
// seconds remaining, keeping incoming notes unobscured. No GO label is added.
inline float pcCountdownAlpha(Seconds remaining) noexcept {
    return remaining>3.0?1.0f:std::clamp(static_cast<float>(remaining)-2.0f,0.0f,1.0f);
}
// The PC runtime's Double.ToString("F0") rounds midpoint values away from
// zero (verified at 0.5, 1.5, 2.5 and 10.5). printf("%.0f") uses ties-to-even.
inline double pcCountdownSeconds(Seconds remaining) noexcept {
    return std::round(remaining);
}
inline int pcCountdownMeasures(const PcCountdown& range,const std::vector<Seconds>& measures,Seconds now) noexcept {
    const auto next=std::upper_bound(measures.begin(),measures.end(),now);
    return range.endMeasure-std::max(0,int(next-measures.begin())-1);
}
}
