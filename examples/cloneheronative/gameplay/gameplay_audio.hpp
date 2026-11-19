// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
namespace rhythm {
// BassAudioManager's named stem groups. Backing/song/crowd never belong to
// an individual guitar/bass/rhythm/drum controller's miss gate.
constexpr bool controlledStem(int instrument,size_t stem) noexcept {
    if(instrument>=4 && instrument<=7)instrument=instrument==5?1:instrument==6?2:0;
    return instrument==0 ? stem==0 : instrument==1 ? stem==1 :
        instrument==2 ? stem==2 : instrument==3 ? stem>=6 && stem<=10 : false;
}
constexpr float missStemGain(int instrument,size_t stem,bool muted,float missVolume) noexcept {
    return muted && controlledStem(instrument,stem) ? missVolume : 1.0f;
}
}
