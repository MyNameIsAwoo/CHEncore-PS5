// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/controller_navigation.hpp"
#include <cassert>
#include <cstdio>
int main() {
    using namespace rhythm::controls;
    ControllerProfile profile;profile.source=Source::RawHid;profile.reportLength=27;
    Locator up;up.kind=Kind::Hat;up.byte=2;up.value=0;
    Locator down=up;down.value=4;
    profile.bindings={{"strum_up",up},{"strum_down",down}};
    uint8_t report[27]{};
    constexpr uint8_t expected[]{1,9,8,10,2,6,4,5,0};
    for(unsigned i=0;i<9;++i) {report[2]=i;assert(rawMenuHat(profile,report,27)==expected[i]);}
    report[2]=2;assert(rawMenuHat(profile,report,27)==MenuHatRight);
    report[2]=6;assert(rawMenuHat(profile,report,27)==MenuHatLeft);
    report[2]=255;assert(!rawMenuHat(profile,report,27));
    assert(!rawMenuHat(profile,nullptr,27));assert(!rawMenuHat(profile,report,2));
    profile.source=Source::Native;assert(!rawMenuHat(profile,report,27));
    profile.source=Source::RawHid;profile.bindings[1].input.byte=3;assert(!rawMenuHat(profile,report,27));
    profile.bindings[1].input.byte=2;profile.bindings[1].input.value=2;assert(!rawMenuHat(profile,report,27));
    std::puts("PASS raw guitar hat: all cardinals/diagonals, neutral, bounds and unsupported-layout rejection");
}
