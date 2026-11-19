// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/menu_stick.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
int main() {
    rhythm::MenuStick s;
    s.update(0,.54f,true);assert(!s.x&&!s.y);
    s.update(0,.6f,true);assert(s.y==1);
    s.update(0,.4f,true);assert(s.y==1);
    s.update(0,.3f,true);assert(!s.y);
    s.update(.9f,-.6f,false);assert(!s.x&&s.y==-1);
    s.update(.9f,0,false);assert(!s.x&&!s.y);
    s.update(-.8f,.6f,true);assert(s.x==-1&&!s.y);
    s.update(.7f,0,true);assert(s.x==1&&!s.y);
    s.update(0,-.8f,true);assert(!s.x&&s.y==-1);
    s.update(std::numeric_limits<float>::quiet_NaN(),0,true);assert(!s.x&&!s.y);
    puts("PASS menu stick dead zone, hysteresis, direction changes and vertical-only main menu");
}
