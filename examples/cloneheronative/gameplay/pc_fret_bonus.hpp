// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>

namespace rhythm {
// GuitarEngine 0x180FD0770 / ghost path 0x180FD1D30; backing meter
// 0x182188730 / 0x182188830 defaults to 96, stages of 32, -8 per ghost,
// +1 per legato gem. This subtotal never contributes to star rank.
class PcFretBonus {
public:
    [[nodiscard]] unsigned charge() const noexcept {return static_cast<unsigned>(charge_);}
    void reset() noexcept {charge_=96;}
    void ghosts(unsigned count) noexcept {
        charge_=count>=12?0:std::max(0,charge_-int(count)*8);
    }
    unsigned award(bool eligible,bool legato,unsigned objects=1) noexcept {
        unsigned bonus=0;
        for(unsigned i=0;i<objects;++i) {
            if(eligible) bonus+=std::min(charge_/32,2);
            if(legato) charge_=std::min(charge_+1,96);
        }
        return bonus;
    }
private:
    int charge_{96};
};
}
