// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <array>
namespace rhythm {
// User-selectable tint on the existing source kick sprites. The PC orange
// remains the default; this low-saturation purple leaves source alpha intact.
inline constexpr std::array<float,3> drumPurpleKick(bool highlight=false) noexcept {
    return highlight?std::array<float,3>{190.f/255,173.f/255,206.f/255}:
                     std::array<float,3>{137.f/255,115.f/255,161.f/255};
}

// The PC compatibility conversion (0x18216D6E0) leaves kick, red,
// blue/green toms and yellow/orange cymbals in canonical engine masks.
// A five-lane view places those masks on R/Y/B/O/G without changing scoring.
inline constexpr unsigned fiveLaneDrumPad(unsigned engineLane) noexcept {
    constexpr unsigned pads[]{0,1,2,3,5,2,3,4};
    return engineLane<8?pads[engineLane]:6;
}
inline constexpr unsigned fiveLaneDrumSource(unsigned visualPad) noexcept {
    constexpr unsigned sources[]{0,1,2,3,3};
    return visualPad>=1 && visualPad<=5?sources[visualPad-1]:0;
}
inline constexpr float drumPadX(unsigned index,unsigned count,float first) noexcept {
    return first*(1.f-2.f*float(index)/float(count-1));
}
inline constexpr bool fiveLaneDrumCymbal(unsigned engineLane) noexcept {
    return engineLane==5 || engineLane==7;
}

// BaseFretAnimator 0x180138B40 / 0x1801388A0 and DrumsFretAnimator
// 0x1803550B0. The source pulse constant is a DOUBLE, not the float 2.0
// in its low four bytes. Sample once per rendering frame, like the PC.
class PcDrumFretMotion {
public:
    void reset() noexcept {amount_=sign_=lastHit_=0;}
    void strike(double time) noexcept {amount_=0.009999999776482582;sign_=1;lastHit_=time;}
    float sample(double time) noexcept {
        if(amount_<=1e-5 && amount_>-1e-5){amount_=0;return 0;}
        amount_=amount_-amount_*(time-lastHit_)*0.34*0.1599999964237213;
        const double period=amount_+amount_;
        const double raw=time*0.34-std::floor(time*0.34/period)*period;
        const double phase=raw<0?0:raw>period?period:raw;
        const auto value=static_cast<float>((amount_-std::abs(phase-amount_))*sign_);
        return std::isfinite(value)?value:0.0f;
    }
private:
    double amount_{},sign_{},lastHit_{};
};
}
