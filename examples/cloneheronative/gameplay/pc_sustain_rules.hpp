// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>

namespace rhythm {
struct PcSustainGem {
    std::int64_t tick{},length{};
    unsigned mask{}; // native five-fret bits, bit 5 Open
    bool extended{},disjoint{};
};
// Raw normalization 0x1805E6630: overlapping heads on the same fret
// transfer the remainder to the later gem and truncate the earlier one.
inline void normalizePcSustainLengths(std::vector<PcSustainGem>& gems) {
    std::array<std::size_t,6> last;
    last.fill(gems.size());
    std::array<std::int64_t,6> ends{};
    for(std::size_t i=0;i<gems.size();++i) {
        auto& gem=gems[i];
        for(unsigned lane=0;lane<6;++lane) if(gem.mask==(1u<<lane)) {
            auto end=gem.tick+gem.length;
            if(last[lane]!=gems.size() && gem.tick<ends[lane]) {
                gems[last[lane]].length=gem.tick-gems[last[lane]].tick;
                end=std::max(end,ends[lane]);
                gem.length=end-gem.tick;
                last[lane]=i;ends[lane]=end;
            } else if(ends[lane]<end) {last[lane]=i;ends[lane]=end;}
        }
    }
}
// Raw grouping 0x182154BD0, runtime conversion 0x1820BB710. Ordinary
// equal-length chords are one runtime object; differing lengths retain
// independent linked children. Overlap marks both objects ExtendedSustain.
inline void classifyPcSustains(std::vector<PcSustainGem>& gems) {
    for(auto& gem:gems) gem.extended=gem.disjoint=false;
    for(std::size_t begin=0;begin<gems.size();) {
        auto end=begin+1;
        while(end<gems.size() && gems[end].tick==gems[begin].tick) ++end;
        bool disjoint=false;
        for(auto i=begin+1;i<end;++i) disjoint|=gems[i].length!=gems[begin].length;
        for(auto i=begin;i<end;++i) gems[i].disjoint=gems[i].extended=disjoint;
        begin=end;
    }
    for(std::size_t i=0;i<gems.size();++i) {
        if(gems[i].length<=0) continue;
        const auto end=gems[i].tick+gems[i].length;
        for(auto j=i+1;j<gems.size() && gems[j].tick<=end;++j)
            if((gems[i].tick<gems[j].tick && gems[j].tick<end) || gems[j].length==0)
                gems[i].extended=gems[j].extended=true;
    }
    // A consolidated equal-length chord shares the runtime flag.
    for(std::size_t begin=0;begin<gems.size();) {
        auto end=begin+1;bool extended=gems[begin].extended;
        while(end<gems.size() && gems[end].tick==gems[begin].tick) extended|=gems[end++].extended;
        if(!gems[begin].disjoint) for(auto i=begin;i<end;++i) gems[i].extended=extended;
        begin=end;
    }
}
// Guitar hit 0x180FCC920: retire before incrementing the new chord streak.
constexpr bool pcHeadStopsTail(bool nextExtended,unsigned nextMask,unsigned tailMask) noexcept {
    return !nextExtended || (nextMask&tailMask)!=0;
}
// 0x180FD0C20: ExtendedSustain holds its own gem, regardless of other frets.
constexpr bool pcExtendedTailHeld(unsigned gemMask,unsigned pressedMask,unsigned openLane=5) noexcept {
    return (gemMask&(1u<<openLane)) || (gemMask&pressedMask)==gemMask;
}
}
