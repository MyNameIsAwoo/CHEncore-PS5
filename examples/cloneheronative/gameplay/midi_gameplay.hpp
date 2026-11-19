// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include "midi_drums.hpp"
#include "drum_session.hpp"
namespace rhythm::midi {
// MIDI supplies fresh strike events directly to the existing PC-derived drum
// session. Never run these through held-button edge detection or menu filters.
inline DrumInput gameplayInput(const MappedStrike& event) noexcept {
    DrumInput input;
    if(!event.mask || event.mask>255 || (event.mask&(event.mask-1)))return input;
    input.strikes=event.mask;
    if(event.mask!=1 && event.velocity>=0){
        const int lane=pc_drums::velocityLane(event.mask);
        if(lane>=0 && lane<int(input.velocities.size()))input.velocities[lane]=std::clamp(event.velocity,0,127);
    }
    return input;
}
}
