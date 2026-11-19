// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/pc_drums.hpp"
#include <cassert>
#include <cstdio>
int main() {
    assert(rhythm::pc_drums::fiveLaneInput(4)==32);
    assert(rhythm::pc_drums::fiveLaneInput(256)==128);
    assert(rhythm::pc_drums::fiveLaneInput(1|2|4|8|16|256)==(1|2|32|8|16|128));
    unsigned note,chord;
    while(std::scanf("%u %u",&note,&chord)==2)
        std::printf("%u\n",rhythm::pc_drums::fiveLaneChartMask(note,chord));
}
