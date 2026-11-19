// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "../examples/cloneheronative/gameplay/pc_drums.hpp"
#include <vector>
#include <cassert>
#include <cstdio>
struct Gem{int64_t tick;uint16_t drumMask;uint32_t drumFlags{};};
int main(){
    using namespace rhythm::pc_drums;
    std::vector<Gem> sparse{{192,1},{288,2},{384,4},{480,8},{576,16},{768,32},{960,64},{1152,128}};
    markRoll(sparse,192,500,false,192);markRoll(sparse,700,1000,true,192);
    for(const auto& n:sparse)assert(n.drumFlags==0);
    assert(explicitFillHead(sparse,1200,1536,192)==0);
    std::vector<Gem> rollNotes{{192,2},{216,2},{240,1},{240,4},{264,2}};
    markRoll(rollNotes,192,264,false,192);
    assert(rollNotes[0].drumFlags==roll && rollNotes[1].drumFlags==roll);
    assert(rollNotes[2].drumFlags==0 && rollNotes[3].drumFlags==0);
    assert(rollNotes[4].drumFlags==(roll|rollEnd));
    assert(explicitFillHead(rollNotes,0,240,192)==3);
    assert(explicitFillHead(rollNotes,0,10,192)==0);
    std::vector<Gem> lastChord{{192,1},{192,32},{192,128}};
    markRoll(lastChord,192,960,true,192);
    assert(lastChord[0].drumFlags==0);
    assert(lastChord[1].drumFlags==(roll|doubleRoll));
    assert(lastChord[2].drumFlags==(roll|doubleRoll|rollEnd));
    std::puts("PASS source roll-start filtering, selected pad masks, inclusive end and explicit-fill fallback");
}
