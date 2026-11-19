// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
namespace rhythm {
// BaseMenu input helper 0x1802568F0, ctor 0x1802570E0, Update 0x180256CD0.
// Unscaled initial delay .3s, repeat .026s; after 2s the interval halves.
// Returns repeats only: callers retain ordinary edges for confirm/cancel/etc.
class MenuRepeat {
public:
    void reset() noexcept {held_=0;context_=-1;delay_=interval_=0;acceleration_=2;}
    uint32_t update(uint32_t held,uint32_t down,float dt,int context,bool enabled=true) noexcept {
        if(!enabled) {reset();return 0;}
        dt=std::max(0.0f,dt);
        uint32_t result=0;
        if(context_!=context || held_!=held || (held&down)) {
            context_=context;held_=held;delay_=0.3f;interval_=0;acceleration_=2;
        } else if(held && delay_<=0 && interval_<=0) {
            result=held;interval_=acceleration_<0?0.013f:0.026f;
        }
        if(!held) {delay_=interval_=0;acceleration_=2;}
        else {delay_-=dt;interval_-=dt;acceleration_-=dt;}
        return result;
    }
private:
    uint32_t held_{};int context_{-1};
    float delay_{},interval_{},acceleration_{2};
};
}
