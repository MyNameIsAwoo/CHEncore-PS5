// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/pc_player_camera.hpp"
#include <cassert>
#include <cstdio>
int main() {
    const float heights[]{1,1,1.73f,2.1925f},depths[]{3.97f,3.97f,5.83f,6.91f};
    for(unsigned count=1;count<=4;++count) {
        const auto camera=rhythm::pcPlayerCamera(count);
        assert(std::abs(camera.height-heights[count-1])<1e-6f);
        assert(std::abs(camera.z-depths[count-1])<1e-6f);
        float end=0;
        for(unsigned seat=0;seat<count;++seat) {
            const auto viewport=rhythm::pcPlayerViewport(count,seat,16.f/9);
            assert(std::abs(viewport.x-end)<1e-6f);
            assert(viewport.y==0 && viewport.height==1);
            end=viewport.x+viewport.width;
        }
        assert(end==1);
    }
    assert(rhythm::pcScoreHudAnchor(1).y==-1.38f);
    assert(rhythm::pcScoreHudAnchor(2).x==-6 && rhythm::pcScoreHudAnchor(2).y==2);
    assert(rhythm::pcScoreHudAnchor(3).x==.9f && rhythm::pcScoreHudAnchor(4).y==3);
    for(int renderer:{18156,15969,17098,14844}) {
        const float x=2,center=3;
        assert(rhythm::pcMultiplierTextX(x,center,false,renderer)==x);
        const auto mirrored=rhythm::pcMultiplierTextX(x,center,true,renderer);
        assert(mirrored==(renderer==14844?2:4));
        if(renderer!=14844) {
            // Final highway X reflection retains the lettering's positive X
            // direction; it moves the complete group to the opposite rail.
            const auto right=rhythm::pcMultiplierTextX(x+1,center,true,renderer);
            assert(-right>-mirrored);
        }
    }
    std::puts("PASS PC one-to-four-player camera/viewport and readable lefty multiplier sampling");
}
