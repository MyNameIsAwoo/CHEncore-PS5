// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/menu_repeat.hpp"
#include <cassert>
#include <cstdio>
int main(){
 rhythm::MenuRepeat r;
 constexpr float dt=1.f/1000;
 assert(r.update(1,1,dt,0)==0);
 unsigned slow=0,fast=0;
 for(int i=1;i<3000;i++){
  const auto move=r.update(1,0,dt,0);
  if(i<299)assert(!move);
  if(i>=400&&i<1400)slow+=move!=0;
  if(i>=2100)fast+=move!=0;
 }
 assert(slow>=37&&slow<=39&&fast>=65&&fast<=70);
 assert(!r.update(0,0,dt,0));
 assert(!r.update(2,2,dt,0));
 for(int i=0;i<250;i++)assert(!r.update(2,0,dt,0));
 assert(!r.update(2,0,dt,1));
 assert(!r.update(2,0,dt,1,false));
 assert(!r.update(2,0,dt,1));
 std::puts("PASS: PC .3s initial delay, .026/.013s repeat, release/direction/context reset and gameplay exclusion");
}
