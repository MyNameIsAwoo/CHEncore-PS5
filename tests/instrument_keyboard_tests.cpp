// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/instrument_keyboard.hpp"
#include <cassert>
#include <cstdio>
int main() {
    rhythm::InstrumentKeyboard k;k.begin("",24);
    assert(k.open && k.key==0);
    k.move(-1);assert(k.key==9);k.move(1);assert(k.key==0);
    k.strum(-1);assert(k.key==43 && k.select()==-1);
    k.strum(1);assert(k.key==0);k.key=10;k.select();assert(k.text=="q");
    k.upper=true;k.select();assert(k.text=="qQ");
    k.key=40;k.select();assert(k.text=="qQ ");
    k.erase();assert(k.text=="qQ");
    k.key=42;assert(k.select()==1 && k.text=="qQ");
    k.key=41;k.select();assert(k.text.empty());
    k.begin("caf\xc3\xa9",96);k.erase();assert(k.text=="caf");
    k.begin("caf\xc3\xa9",4);assert(k.text=="caf");
    k.begin("abc",3);k.key=10;k.select();assert(k.text=="abc");
    for(int i=0;i<44;++i)for(int direction:{-1,1}) {k.key=i;k.row(direction);assert(k.key>=0 && k.key<44);}
    k.key=39;k.row(1);assert(k.key==43);k.row(1);assert(k.key==9);
    for(int column=0;column<10;++column){k.setKey(column);k.row(-1);k.row(1);assert(k.key==column);}
    k.setKey(24);k.move(1);assert(k.key==25);k.row(1);assert(k.key==35);
    k.row(1);assert(k.key==41);k.row(-1);assert(k.key==35);
    k.begin("unchanged",96);k.key=43;assert(k.select()==-1 && k.text=="unchanged");
    puts("PASS instrument text limits, UTF-8 deletion, navigation and explicit accept/cancel");
}
