// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
namespace rhythm::controls {
// Device slots identify input sources; seats identify player/UI order.
// A caller moves its per-player state with the same erase/compact operation.
struct PlayerSeats {
    std::array<int,4> slotOfSeat{{-1,-1,-1,-1}};
    std::array<int,8> seatOfSlot{{-1,-1,-1,-1,-1,-1,-1,-1}};
    int count{};
    int join(int slot) noexcept {
        if(slot<0 || slot>=8)return -1;
        if(seatOfSlot[slot]>=0)return seatOfSlot[slot];
        if(count==4)return -1;
        const int seat=count++;slotOfSeat[seat]=slot;seatOfSlot[slot]=seat;return seat;
    }
    void erase(int seat) noexcept {
        if(seat<0 || seat>=count)return;
        seatOfSlot[slotOfSeat[seat]]=-1;
        for(int i=seat;i+1<count;++i){slotOfSeat[i]=slotOfSeat[i+1];seatOfSlot[slotOfSeat[i]]=i;}
        slotOfSeat[--count]=-1;
    }
};
}
