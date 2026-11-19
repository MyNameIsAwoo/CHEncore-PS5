// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace rhythm {
// Filters fresh menu edges only. The first edge is immediate; MenuRepeat adds
// intentional held-navigation repeats afterwards. Confirm/Start/menu actions
// use a 220-ms guard across page transitions. No note-judging input enters here.
class MenuPressFilter {
public:
    void reset() noexcept {seen_=0;last_.fill(0);}
    uint32_t update(uint32_t edges,double now,double interval=.075,
                    uint32_t vertical=0x50,uint32_t horizontal=0xa0) noexcept {
        if(!std::isfinite(now))return 0;
        if(!(interval>0))return edges;
        uint32_t accepted=0;
        const uint32_t navigation=vertical|horizontal;
        for(unsigned bit=0;bit<32;++bit) {
            const uint32_t mask=uint32_t(1)<<bit;if(!(edges&mask))continue;
            const uint32_t group=(mask&vertical)?vertical:(mask&horizontal)?horizontal:mask;
            bool blocked=false;
            for(unsigned peer=0;peer<32;++peer)if((group&(uint32_t(1)<<peer)) && (seen_&(uint32_t(1)<<peer)) &&
                now>=last_[peer] && now-last_[peer]<((mask&navigation)?interval:std::max(interval,.220)))blocked=true;
            if(!blocked)accepted|=mask;
        }
        // Evaluate a simultaneous chord against the old history, then commit.
        for(unsigned bit=0;bit<32;++bit)if(accepted&(uint32_t(1)<<bit))last_[bit]=now;
        seen_|=accepted;return accepted;
    }
private:
    std::array<double,32> last_{};uint32_t seen_{};
};
}
