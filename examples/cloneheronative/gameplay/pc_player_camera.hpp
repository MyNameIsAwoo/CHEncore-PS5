// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>

namespace rhythm {
struct PcPlayerCamera {
    float factor,height,z,targetZ,uiScale,uiOffset;
};
// GameManager 0x180287C20 calls PlayerCamera 0x1800ADA40, whose
// live Update 0x1800AEC40 calls 0x1800AE000 and viewport 0x1800ADE50.
// Other obfuscated duplicate methods in the class are not this call path.
inline PcPlayerCamera pcPlayerCamera(unsigned players) noexcept {
    const float count=float(std::clamp(players,1u,4u));
    const float f=std::max(0.0f,std::min(2*(count-2),count-1));
    return {f,1+.3f*f+.0325f*f*f,3.97f+.83f*f+.05f*f*f,
        5.545f+.825f*f,1-.108f*f,8.3f*f};
}
struct PcPlayerViewport {float x,y,width,height;};
inline PcPlayerViewport pcPlayerViewport(unsigned players,unsigned ordinal,float aspect) noexcept {
    const float count=float(std::clamp(players,1u,4u));
    const float height=aspect/(16.0f/9.0f);
    return {float(std::min(ordinal,unsigned(count)-1))/count,1-height,1/count,height};
}
struct PcScoreHudAnchor {float x,y;};
// GameManager 0x18028A220. Single-player uses the saved/default HUD position;
// two players use (-6,2); three/four use (.9,3). HUDCam is orthographic size5
// and the source HUD root scale is4.25 in level2.
constexpr PcScoreHudAnchor pcScoreHudAnchor(unsigned players) noexcept {
    return players==2?PcScoreHudAnchor{-6,2}:players>=3?PcScoreHudAnchor{.9f,3}:PcScoreHudAnchor{.08f,-1.38f};
}
// Counter-reflect the complete xN group inside its mirrored circle. This
// preserves both numeral shape and x-before-number order for lefty players.
constexpr float pcMultiplierTextX(float x,float circleCenter,bool lefty,int renderer) noexcept {
    return lefty && (renderer==18156 || renderer==15969 || renderer==17098)?2*circleCenter-x:x;
}
}
