// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/controller_navigation.hpp"
#include "gameplay/raw_input_queue.hpp"
#include "gameplay/usb_source_slots.hpp"
#include "gameplay/player_seats.hpp"
#include "gameplay/menu_press_filter.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using namespace rhythm::controls;
 // Two identical drum models must still have independent reports and queues.
 std::array<ControllerProfile,4> profiles;
 std::string error;
 const char* names[]{"0738_8262","0738_8262","12ba_0100","12ba_0120"};
 for(unsigned i=0;i<4;++i)assert(loadProfile((std::string("assets/controller_profiles/")+names[i]+".json").c_str(),profiles[i],error));
 std::array<RawInputReport,4> previous{};
 std::array<RawInputQueue,4> histories;
 auto publish=[&](unsigned source,const RawInputReport& report){
  const auto& old=previous[source];
  if(!old.length || !sameControllerInput(profiles[source],old.bytes.data(),old.length,report.bytes.data(),report.length))histories[source].push(report);
  previous[source]=report;
 };
 for(unsigned i=0;i<4;++i){RawInputReport neutral;neutral.length=profiles[i].reportLength;
  if(i<2){neutral.bytes[0]=1;neutral.bytes[5]=8;}else neutral.bytes[2]=8;
  publish(i,neutral);
 }
 for(unsigned step=0;step<128;++step){
  auto counter=previous[0];counter.bytes[12]++;publish(0,counter);
  for(unsigned source=1;source<4;++source){auto report=previous[source];report.receivedUs=1000+step*2000;
   if(source==1)report.bytes[5]=8|((step%2)?0:32);
   else if(source==2)report.bytes[0]=(step%2)?0:2;
   else report.bytes[0]=(step%2)?0:32;
   publish(source,report);
  }
 }
 RawInputBatch batch;histories[0].drain(batch);assert(batch.count==1 && !batch.dropped);
 for(unsigned source=1;source<4;++source){histories[source].drain(batch);assert(batch.count==129 && !batch.dropped);
  for(unsigned n=1;n<batch.count;++n){const auto& report=batch.reports[n];
   if(source==1)assert(rawMenuControls(profiles[source],report.bytes.data(),report.length).colors==((n%2)?1:0));
   if(source==2)assert(applyProfile(profiles[source],{},report.bytes.data(),report.length).frets==((n%2)?1:0));
   if(source==3)assert(rawMenuControls(profiles[source],report.bytes.data(),report.length).colors==((n%2)?16:0));
  }
 }
 // Overflow on a noisy source cannot drain or reset another source's queue.
 RawInputReport edge=previous[1];histories[1].push(edge);
 for(unsigned i=0;i<300;++i){edge.receivedUs=i;histories[0].push(edge);}
 histories[0].drain(batch);assert(batch.count==256 && batch.dropped==44);
 histories[1].drain(batch);assert(batch.count==1 && !batch.dropped);
 std::array<bool,16> active{};std::array<int,16> slots;slots.fill(-1);
 active[5]=true;assignUsbSources(active,slots);assert(slots[5]==0);
 active[0]=active[10]=active[15]=true;assignUsbSources(active,slots);
 assert(slots[5]==0 && slots[0]==1 && slots[10]==2 && slots[15]==3);
 active[0]=false;assignUsbSources(active,slots);
 assert(slots[5]==0 && slots[10]==2 && slots[15]==3);
 std::array<rhythm::MenuPressFilter,4> filters;
 for(unsigned player=0;player<4;++player)assert(filters[player].update(0x4000,player*.001)==0x4000);
 assert(!filters[0].update(0x4000,.01));assert(filters[1].update(0x2000,.011)==0x2000);
 PlayerSeats seats;assert(seats.join(7)==0 && seats.join(4)==1 && seats.join(6)==2 && seats.join(5)==3);
 const std::array<int,4> expectedSeats{7,4,6,5};
 assert(seats.slotOfSeat==expectedSeats);
 std::puts("PASS four interleaved USB streams, identical models, counter-only noise, overflow isolation, stable slots, per-player filters and join order");
}
