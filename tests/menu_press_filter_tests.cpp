// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/menu_press_filter.hpp"
#include "gameplay/menu_repeat.hpp"
#include <cassert>
#include <cstdio>
int main() {
    rhythm::MenuPressFilter filter;
    constexpr uint32_t up=0x10,down=0x40,green=0x4000,red=0x2000;
    assert(filter.update(down,0)==down); // no delay on the first press
    assert(!filter.update(down,.016));
    assert(!filter.update(up,.033)); // strum rebound in the opposite direction
    assert(filter.update(green,.033)==green); // independent actions remain responsive
    assert(filter.update(red,.034)==red);
    assert(!filter.update(green,.050));
    assert(filter.update(down,.075)==down);
    assert(!filter.update(green,.109)); // bounce cannot confirm through another page
    assert(filter.update(green,.260)==green);
    assert(!filter.update(green,.400));
    assert(filter.update(green,.481)==green);
    rhythm::MenuPressFilter other;
    assert(other.update(down,.02)==down); // independent player history
    filter.reset();assert(filter.update(down,.02)==down);
    assert(filter.update(down,.021,0)==down);
    filter.reset();rhythm::MenuRepeat repeat;
    unsigned moves=0;
    for(int frame=0;frame<1000;++frame) {
        const uint32_t bounced=frame==0||frame==16||frame==32?down:0;
        const auto edge=filter.update(bounced,frame*.001);
        const auto generated=repeat.update(down,edge,.001f,0);
        if(frame<299)assert(!generated);
        moves+=bool(edge|generated);
    }
    assert(moves>20 && moves<30); // holding still scrolls after the PC initial delay
    std::puts("PASS menu bounce rejection, immediate first press, independent players/actions and held repeat");
}
