// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace rhythm {
// Six-fret comparison recovered from GameAssembly 0x1820F4510. Source bit 0
// is Open; W1/W2/W3/B1/B2/B3 occupy bits 1..6, respectively.
constexpr bool pcSixFretMatch(unsigned note,unsigned pressed,bool chord,bool looseChord=false) noexcept {
    if(note==pressed)return true;
    if(note&1u){if(pressed!=1u)return (note&0xfffeu)==pressed;if(!(note&0xfffeu))return true;}
    if(looseChord && (note&pressed)==note)return true;
    const unsigned difference=note^pressed;
    if(chord || note>=pressed || ((pressed-difference)&note)!=(pressed-difference) || note==0x10 || note==2)return false;
    if(note==0x20)return !(difference&0x48) && !(difference&4);
    if(note==0x40)return !(difference&8);
    if(note==4)return !(difference&0x48) && !(difference&0x20);
    if(note==8)return !(difference&0x40);
    return false;
}
constexpr int sixFretChartLane(int value) noexcept {return value==8?5:value==7?6:value>=0 && value<=4?value:-1;}
constexpr bool guitarTrackForController(int instrument,int controller) noexcept {
    return controller==1?instrument>=4 && instrument<8:controller>=2?instrument==3:instrument>=0 && instrument<3;
}
constexpr float sixFretLaneX(unsigned lane) noexcept {return (int(lane%3)-1)*1.5f;}
}
