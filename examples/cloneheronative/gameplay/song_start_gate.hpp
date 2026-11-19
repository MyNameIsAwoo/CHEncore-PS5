// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
namespace rhythm {
// Loading and first-frame preparation belong outside the song timeline.
// Reset on restart; begin only after the first prepared frame is submitted.
struct SongStartGate {
    bool waiting{},fresh{};
    double presentedAt{};
    void arm() noexcept {waiting=true;fresh=false;}
    void presented(double now) noexcept {if(waiting){waiting=false;fresh=true;presentedAt=now;}}
    double delta(double frameDelta,double now) noexcept {
        if(waiting)return 0;
        if(fresh){fresh=false;return std::max(0.0,now-presentedAt);}
        return frameDelta;
    }
};
}
