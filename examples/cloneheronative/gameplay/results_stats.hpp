// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace rhythm {
// Read-only score ledger: BaseEngine head award 0x1820F3250, sustain award
// 0x1820F2260 and hit/squeeze flags 0x1820F2790. It never changes judgement.
struct ResultsStats {
    uint64_t notes{},sustains{},combo{},starPower{};
    int64_t squeezeScore{};
    unsigned overstrums{},ghosts{},activations{},positiveSqueezes{},negativeSqueezes{};
    unsigned accents{},ghostPads{};
    double barsFilled{},spSeconds{},spStart{-1e30},spEnd{-1e30},lastSpSample{},lastSpEnd{-1e30};
    void award(unsigned raw,unsigned streak,bool active,bool sustain=false,bool positive=false,bool negative=false) noexcept {
        const auto tier=std::min(4u,1u+streak/10u);
        (sustain?sustains:notes)+=raw;
        combo+=uint64_t(raw)*(tier-1);
        if(active)starPower+=uint64_t(raw)*tier;
        if(!sustain) {
            if(positive){++positiveSqueezes;squeezeScore+=int64_t(raw)*tier;}
            if(negative){++negativeSqueezes;squeezeScore-=int64_t(raw)*(tier-1);}
        }
    }
    void advanceSp(double time,bool active,double end) noexcept {
        if(active && time>=lastSpSample)spSeconds+=std::max(0.0,std::min(time,end)-lastSpSample);
        if(active && time>=end)lastSpEnd=end;
        lastSpSample=time;
    }
};
}
