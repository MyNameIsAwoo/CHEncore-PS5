// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/raw_input_queue.hpp"
#include "gameplay/controller_profile.hpp"
#include "gameplay/guitar_session.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using namespace rhythm;using namespace rhythm::controls;
 ControllerProfile p;std::string error;assert(loadProfile("assets/controller_profiles/0e6f_0249.json",p,error));
 std::array<uint8_t,64> idle{};idle[0]=1;idle[1]=idle[2]=128;idle[8]=15;
 auto metadata=idle;metadata[12]=17;metadata[63]=33;assert(sameControllerInput(p,idle.data(),64,metadata.data(),64));
 auto fret=metadata;fret[43]=1;assert(!sameControllerInput(p,idle.data(),64,fret.data(),64));
 auto strum=fret;strum[8]=0;assert(!sameControllerInput(p,fret.data(),64,strum.data(),64));
 assert(!sameControllerInput(p,fret.data(),64,strum.data(),63));
 for(unsigned fps:{4u,30u,60u,120u}) {
  SongChart chart;for(unsigned i=0;i<100;++i)chart.notes.push_back({1+i*.05,0,1});
  GuitarSession game(chart);game.setStrumDebounce(.005);(void)game.update({},0);
  RawInputQueue queue;RawInputBatch batch;auto previous=idle;double previousTime=0;unsigned overflow=0;
  for(unsigned ms=0;ms<=6200;++ms) {
   const double now=ms*.001;auto current=idle;current[43]=1;current[12]=uint8_t(ms);current[63]=uint8_t(ms*7);
   if(ms>=1000 && ms<6000 && (ms-1000)%50<2)current[8]=((ms-1000)/50)&1?4:0;
   if(!sameControllerInput(p,current.data(),64,previous.data(),64))queue.push({uint64_t(ms)*1000,64,current});
   previous=current;
   if((ms*fps)/1000!=((ms?ms-1:0)*fps)/1000 || ms==6200) {
    queue.drain(batch);overflow+=batch.dropped;
    for(unsigned i=0;i<batch.count;++i){const auto& report=batch.reports[i];const auto input=applyProfile(p,{},report.bytes.data(),report.length);
     const double at=rawInputSongTime(report.receivedUs,uint64_t(ms)*1000,now,previousTime,1);
     (void)game.update({input.frets,input.strumUp,input.strumDown},at);previousTime=at;
    }
    const auto input=applyProfile(p,{},current.data(),64);(void)game.update({input.frets,input.strumUp,input.strumDown},now);previousTime=now;
   }
  }
  assert(!overflow && game.hits()==100 && game.misses()==0);
 }
 std::puts("PASS 100 two-ms alternate strums at 4/30/60/120 FPS: zero dropped or duplicated hits; counter-only packets ignored, real press/releases preserved");
}
