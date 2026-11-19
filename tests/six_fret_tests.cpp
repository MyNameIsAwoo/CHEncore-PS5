// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/pc_six_fret.hpp"
#include "gameplay/guitar_session.hpp"
#include "gameplay/controller_profile.hpp"
#include "gameplay/controller_navigation.hpp"
#include "gameplay/play_menus.hpp"
#include "gameplay/gameplay_audio.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using namespace rhythm;using namespace rhythm::controls;
 for(int type=4;type<8;++type)for(unsigned stem=0;stem<14;++stem)
  assert(controlledStem(type,stem)==(stem==unsigned(type==5?1:type==6?2:0)));
 static_assert(sixFretChartLane(8)==5 && sixFretChartLane(7)==6 && sixFretChartLane(5)==-1);
 assert(pcSixFretMatch(4,6,false)); // lower white fret ghosting
 assert(!pcSixFretMatch(4,36,false)); // opposite row blocks ghosting
 assert(!pcSixFretMatch(2,6,false)); // leftmost fret has no lower fret
 assert(pcSixFretMatch(64,80,false));
 assert(!pcSixFretMatch(18,22,true)); // authored bar/chord requires its own mask
 assert(pcSixFretMatch(1,0,false) && !pcSixFretMatch(1,2,false));
 for(int c=0;c<5;++c)for(int t=0;t<8;++t)
  assert(guitarTrackForController(t,c)==(c==1?t>=4:c>=2?t==3:t<3));
 for(auto name:{"1430_07bb","12ba_074b"}) {
  ControllerProfile p;std::string error;assert(loadProfile((std::string("assets/controller_profiles/")+name+".json").c_str(),p,error));
  std::array<uint8_t,64> data{};const bool ps4=p.vid==0x1430;if(ps4)data[0]=1;data[ps4?2:4]=128;data[ps4?5:2]=ps4?8:15;
  assert(applyProfile(p,{},data.data(),p.reportLength).frets==0);
  assert(rawMenuControls(p,data.data(),p.reportLength).colors==0);
  constexpr unsigned bitsPs4[]{16,1,2,32,64,128},bitsPs3[]{1,16,32,2,4,8};
  for(unsigned lane=0;lane<6;++lane) {
   const unsigned byte=ps4?((lane==1||lane==2)?6:5):0;
   const auto neutral=data;data[byte]|=(ps4?bitsPs4:bitsPs3)[lane];data[ps4?2:4]=0;
   const auto state=applyProfile(p,{},data.data(),p.reportLength);assert(state.frets==(1u<<lane) && state.strumUp && !state.strumDown);
   data[ps4?2:4]=255;assert(applyProfile(p,{},data.data(),p.reportLength).strumDown);data=neutral;
  }
 }
 SongChart chart;chart.kind=ChartKind::SixFret;
 chart.notes={{1,.5,32},{2,.25,9},{3,0,4}};chart.notes[1].chord=true;chart.notes[2].tap=true;
 assert(validateChart(chart));GuitarSession session(chart);
 assert(session.update({32,true,false},1));session.update({32,false,false},1.1);
 assert(session.update({9,false,true},2));session.update({9,false,false},2.1);
 assert(session.update({4,false,false},3));assert(session.evaluator().state(2)==NoteState::Hit);
 chart.kind=ChartKind::FiveFret;assert(!validateChart(chart));
 std::puts("PASS six-fret row/chord matching, sixth-fret judging, taps, controller-gated tracks and simultaneous fret/strum reports");
}
