// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
namespace rhythm {
// GameManager coroutine 0x1802B1C50: a 1.5s rollback at 2x speed, disabled
// in Practice and before enough audio has played. Never rejudge the lead-in.
struct PcResume {
    double protectedUntil{},position{},remaining{};bool animated{},catchingUp{};
    void reset() noexcept {*this={};}
    bool begin(double clock,double audio,bool practice) noexcept {
        if(catchingUp && clock<protectedUntil)return false;
        reset();
        if(practice || !std::isfinite(clock) || clock<0 || audio<1.5)return false;
        protectedUntil=clock;position=clock;remaining=1.5;animated=true;catchingUp=true;
        return true;
    }
    bool advance(double dt) noexcept {
        if(!animated || !std::isfinite(dt) || dt<=0)return false;
        const double step=std::min(remaining,dt*2);
        remaining=std::max(0.0,remaining-step);position=protectedUntil-(1.5-remaining);
        if(remaining>1e-9)return false;
        position=protectedUntil-1.5;animated=false;return true;
    }
    bool protectedAt(double clock) noexcept {
        if(animated)return true;
        if(catchingUp && clock>=protectedUntil)catchingUp=false;
        return catchingUp;
    }
};
}
