// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
namespace rhythm::controls {
// Empty receiver channels never reserve one of the game's four USB sources.
// Connected sources keep their slot until unplugged; player seats remain join-order.
template<size_t N> void assignUsbSources(const std::array<bool,N>& active,std::array<int,N>& slots) noexcept {
    std::array<bool,4> used{};
    for(size_t i=0;i<N;++i) {
        if(!active[i] || slots[i]<0 || slots[i]>=4 || used[slots[i]])slots[i]=-1;
        else used[slots[i]]=true;
    }
    for(size_t i=0;i<N;++i)if(active[i] && slots[i]<0)for(int slot=0;slot<4;++slot)if(!used[slot]){used[slot]=true;slots[i]=slot;break;}
}
}
