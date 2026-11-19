// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace rhythm {
// 0x1820F2790 uses float32 percent/truncation for the displayed threshold;
// 0x1820F2470 / 0x1820F39B0 use doubles, nearest/even, then multiply by ten.
// Double 0x183077828 is 10.0. Each chord parent counts once, not per gem.
inline unsigned pcSoloPercent(unsigned hit,unsigned total) noexcept {
    return total?static_cast<unsigned>((float(hit)/float(total))*100.0f):0;
}
inline uint64_t pcSoloBonus(unsigned hit,unsigned total) noexcept {
    if(!total || hit>total || pcSoloPercent(hit,total)<50) return 0;
    const double value=((double(hit)/total-0.5)*2.0)*hit*10.0;
    const auto whole=static_cast<uint64_t>(std::floor(std::max(0.0,value)));
    const double fraction=value-whole;
    return (whole+((fraction>0.5 || (fraction==0.5 && (whole&1)))?1:0))*10;
}
struct PcSolo {
    std::size_t firstHead{},lastHead{};
    double startTime{},endTime{};
    unsigned hits{},judged{};
    bool awarded{},eligible{true};
    [[nodiscard]] unsigned total() const noexcept {return static_cast<unsigned>(lastHead-firstHead+1);}
    void reset(bool enabled=true) noexcept {hits=judged=0;awarded=false;eligible=enabled;}
    uint64_t judge(std::size_t head,bool hit,bool settle=true,bool endGroup=false) noexcept {
        if(!eligible || awarded || head<firstHead || head>lastHead) return 0;
        ++judged;if(hit) ++hits;
        if(!settle || (head!=lastHead && !endGroup)) return 0;
        awarded=true;
        return pcSoloBonus(hits,total());
    }
};
}
