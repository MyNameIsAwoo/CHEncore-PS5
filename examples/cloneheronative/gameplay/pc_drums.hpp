// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#pragma once
#include "song_pipeline.hpp"
#include <array>

namespace rhythm {
namespace pc_drums {
inline constexpr uint32_t activation=0x800,roll=0x4000,doubleRoll=0x8000,rollEnd=0x10000;
inline constexpr uint32_t ghost=0x40000,accent=0x80000,doubleKick=0x100000,cymbal=0x200000;
// Runtime conversion 0x1820B7EA0: snap to a non-kick within ceil(PPQ/16),
// then mark only the selected one/two pad masks. Sparse or late starts are
// ignored; unrelated pads and kicks inside a region are not roll notes.
template<class Gems> void markRoll(Gems& gems,int64_t start,int64_t end,bool dual,int resolution) {
    auto first=std::lower_bound(gems.begin(),gems.end(),start,[](const auto& n,int64_t tick){return n.tick<tick;});
    while(first!=gems.end() && !(first->drumMask&254))++first;
    if(first==gems.end() || first->tick>start+(std::max(resolution,1)+15)/16)return;
    auto after=std::upper_bound(first,gems.end(),end,[](int64_t tick,const auto& n){return tick<n.tick;});
    if(after==first || after-first==1)return;
    uint16_t masks=first->drumMask;
    for(auto n=first;n!=gems.end() && n->tick==first->tick;++n)if(n->drumMask&254)masks=std::min(masks,n->drumMask);
    if(dual){
        // 0x1806051D0 accepts another object at the SAME tick when its
        // index differs. A terminal two-cymbal chord is a valid dual roll.
        auto next=first+1;while(next!=gems.end() && !(next->drumMask&254))++next;
        if(next==gems.end())return;
        const auto tick=next->tick;uint16_t highest=next->drumMask;
        for(;next!=gems.end() && next->tick==tick;++next)if(next->drumMask&254)highest=std::max(highest,next->drumMask);
        masks|=highest;
    }
    auto last=gems.end();
    for(auto n=first;n!=after;++n)if(n->drumMask&masks){n->drumFlags|=roll|(dual?doubleRoll:0);last=n;}
    if(last!=gems.end())last->drumFlags|=rollEnd;
}
// Explicit-fill nearest-head helper 0x180604A70. The source returns its sole
// candidate without applying range bounds, including the first-head fallback
// when a fill extends beyond the final note.
template<class Gems> size_t explicitFillHead(const Gems& gems,int64_t start,int64_t end,int resolution) {
    if(gems.empty())return gems.size();
    const auto next=std::lower_bound(gems.begin(),gems.end(),end,[](const auto& n,int64_t tick){return n.tick<tick;});
    auto previous=next==gems.end()?gems.begin():next;
    while(previous!=gems.begin() && previous->tick>end)--previous;
    const bool hasPrevious=previous->tick<=end;
    auto chosen=next;
    if(next==gems.end())chosen=hasPrevious?previous:gems.end();
    else if(hasPrevious){
        const bool before=previous->tick<start,after=next->tick>end+resolution/4;
        if(before && after)return gems.size();
        if(!before && (after || next->tick-end>end-previous->tick))chosen=previous;
    }
    if(chosen==gems.end())return gems.size();
    auto best=chosen;const auto tick=chosen->tick;
    while(best!=gems.begin() && (best-1)->tick==tick)--best;
    for(auto n=best;n!=gems.end() && n->tick==tick;++n)if(n->drumMask>best->drumMask)best=n;
    return static_cast<size_t>(best-gems.begin());
}
// PC five-lane compatibility conversion 0x18216D6E0. Collision choices use
// the full same-tick chord mask, as returned by 0x182189350.
inline constexpr uint16_t fiveLaneChartMask(uint16_t note,uint16_t chord) noexcept {
    if(note==4)return (chord&(8|64))?2:8;
    if(note==64)return (chord&(16|128))?32:128;
    return note;
}
// Physical GH kit: red/blue/green pads, yellow/orange cymbals and kick.
inline constexpr uint16_t fiveLaneInput(uint16_t pads) noexcept {
    return (pads&~uint16_t(4|256)) | ((pads&4)?32:0) | ((pads&256)?128:0);
}

// The existing DualSense fallback exposes G/R/Y/B/K as five held bits.
// Normalize it once at the adapter boundary; guitar bindings stay independent.
inline uint16_t fallbackPads(uint16_t frets) noexcept {
    return ((frets&16)>>4)|(frets&14)|((frets&1)<<4);
}
class PadEdges {
public:
    void prime(uint16_t held) noexcept {previous_=held;}
    uint16_t sample(uint16_t held) noexcept {const auto fresh=static_cast<uint16_t>(held&~previous_);previous_=held;return fresh;}
private:
    uint16_t previous_{};
};
// Drum input 0x1820D41E0: kick, R/Y/B/G toms, then Y/B/G cymbals.
inline uint16_t inputMask(uint16_t raw,bool pro,bool lefty) noexcept {
    raw&=255;
    if(lefty) raw=(raw&129) | ((raw&2)<<3) | ((raw&16)>>3) |
        ((raw&4)<<1) | ((raw&8)>>1) | ((raw&32)<<1) | ((raw&64)>>1);
    return pro?raw:static_cast<uint16_t>(((raw|((raw>>3)&~2u))&30)|(raw&1));
}
inline int velocityLane(uint16_t mask) noexcept {
    switch(mask) {
    case 2:return 0;case 4:return 1;case 8:return 2;case 16:return 3;
    case 1:return 4;case 32:return 5;case 64:return 6;case 128:return 7;
    default:return 0;
    }
}
inline std::array<int,8> inputVelocities(std::array<int,8> v,bool lefty) noexcept {
    if(lefty) {std::swap(v[0],v[3]);std::swap(v[1],v[2]);std::swap(v[5],v[6]);}
    return v;
}
// 0x1820D3E70, 0x1820D54E0 and BaseEngine neighbor helper 0x1820F35B0.
// Chord siblings share a tick and do not shrink each other's timing window.
inline HitWindow window(const SongChart& chart,size_t index,bool precision) noexcept {
    if(index>=chart.notes.size()) return {};
    const auto at=chart.notes[index].time;
    std::optional<Seconds> previous,next;
    for(size_t i=index;i>0;--i) if(chart.notes[i-1].time<at) {previous=chart.notes[i-1].time;break;}
    for(size_t i=index+1;i<chart.notes.size();++i) if(chart.notes[i].time>at) {next=chart.notes[i].time;break;}
    const double minimum=precision?0.025:0.0375,maximum=precision?0.04:0.085;
    const double ms=(neighborWindowContribution(previous,at,minimum,maximum)+
        neighborWindowContribution(at,next,minimum,maximum))*1000.0;
    const double squared=std::pow(ms,2.0);
    const double full=precision?(13.0-(ms*-0.04-squared*0.0004)*12.4805915)/1000.0:
        ((ms*0.0110924369747899-squared*0.00002628539)*170.0-20.0)/1000.0;
    return {full*0.5,full*0.5};
}
// First representable time outside the inclusive late window. The rounded
// sum can already be outside; advancing it unconditionally adds one ULP.
inline Seconds lateDeadline(Seconds head,Seconds half) noexcept {
    return HitWindow{half,half}.lateDeadline(head);
}
// Drum hit 0x1820D4C80 and score override 0x1820D53A0. Unknown (-1)
// velocity hits normally, but earns no dynamics bonus; Deadly Dynamics rejects it.
inline std::optional<unsigned> hitPoints(uint32_t flags,uint16_t mask,int velocity,unsigned threshold,bool deadly) noexcept {
    const bool dynamics=(flags&(ghost|accent))!=0;
    const bool correct=velocity>=0 && ((flags&ghost)?unsigned(velocity)<threshold:unsigned(velocity)>=threshold);
    if(dynamics && !correct && deadly) return std::nullopt;
    return (50u+((mask&224)?15u:0u))*((dynamics && correct)?2u:1u);
}
}
}
