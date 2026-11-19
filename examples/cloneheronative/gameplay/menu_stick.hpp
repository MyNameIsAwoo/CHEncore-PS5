// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <cmath>
namespace rhythm {
// Menu-only cardinal navigation. Separate engage/release thresholds avoid
// repeated edges when an analog stick rests near the activation threshold.
struct MenuStick {
    int x{},y{};
    void reset() noexcept {x=y=0;}
    void update(float horizontal,float vertical,bool allowHorizontal) noexcept {
        if(!std::isfinite(horizontal)||!std::isfinite(vertical)){reset();return;}
        if(!allowHorizontal)horizontal=0;
        if(x && horizontal*x>.35f && std::abs(horizontal)>=std::abs(vertical)){y=0;return;}
        if(y && vertical*y>.35f && std::abs(vertical)>=std::abs(horizontal)){x=0;return;}
        reset();
        if(std::abs(horizontal)>std::abs(vertical) && std::abs(horizontal)>.55f)x=horizontal<0?-1:1;
        else if(std::abs(vertical)>.55f)y=vertical<0?-1:1;
    }
};
}
