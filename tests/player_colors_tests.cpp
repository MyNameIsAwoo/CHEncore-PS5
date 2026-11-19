// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/player_colors.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
    using namespace rhythm;
    for(int c=0;c<9;++c)assert(regularNoteColor(c,0)==regularNoteColor(c,3));
    assert(regularNoteColor(9,0)==regularNoteColor(9,8));
    for(int i=-800;i<800;++i){
        const auto c=regularNoteColor(9,i*.01),next=regularNoteColor(9,i*.01+.001);
        for(unsigned k=0;k<3;++k){assert(std::isfinite(c[k]) && c[k]>=0 && c[k]<=1);assert(std::abs(c[k]-next[k])<.001);}
    }
    assert(highwayPulse(false,.1f,true,30,5)==HighwayPulse::None);
    assert(highwayPulse(true,.1f,true,30,5)==HighwayPulse::Danger);
    assert(highwayPulse(true,.8f,true,30,5)==HighwayPulse::Solo);
    assert(highwayPulse(true,.8f,false,30,5)==HighwayPulse::Streak);
    assert(highwayPulse(true,.8f,false,0,5)==HighwayPulse::FiveStars);
    assert(highwayPulse(true,.8f,false,0,4)==HighwayPulse::None);
    for(auto kind:{HighwayPulse::Danger,HighwayPulse::Streak,HighwayPulse::FiveStars}){
        auto a=highwayPulseTint(kind,.1),b=highwayPulseTint(kind,.5);assert(a!=b);
        for(float value:a)assert(value>=0 && value<=1);
    }
    assert(highwayPulseTint(HighwayPulse::Solo,.1)==highwayPulseTint(HighwayPulse::Solo,5.7));
    assert(highwayPulseTint(HighwayPulse::Solo,0)[2]>highwayPulseTint(HighwayPulse::Solo,0)[0]);
    puts("PASS pulse priorities/default/bounds; fixed note palettes and smooth bounded eight-second RGB cycle");
}
