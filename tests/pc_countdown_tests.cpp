// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/pc_countdown.hpp"
#include <cassert>
#include <cstdio>
int main() {
    using namespace rhythm;std::vector<Seconds> measures;
    for(int i=0;i<20;++i)measures.push_back(i*2.);
    SongChart chart{{{10.,0.,1}}};assert(pcCountdownRanges(chart,measures).empty());
    chart.notes[0].time=12.;auto ranges=pcCountdownRanges(chart,measures);
    assert(ranges.size()==1 && ranges[0].start==0 && ranges[0].duration==12 && ranges[0].endMeasure==6);
    assert(pcCountdownMeasures(ranges[0],measures,0)==6 && pcCountdownMeasures(ranges[0],measures,4)==4);
    chart.notes={{1.,0.,1},{15.,0.,1}};ranges=pcCountdownRanges(chart,measures);
    assert(ranges.size()==1 && ranges[0].start==1 && ranges[0].duration==14 && ranges[0].endMeasure==7);
    chart.notes[0].duration=5.;assert(pcCountdownRanges(chart,measures).empty());
    chart.notes={{1.,5.,1},{3.,0.,1},{20.,0.,1}};ranges=pcCountdownRanges(chart,measures);
    assert(ranges.size()==1 && ranges[0].start==6 && ranges[0].duration==14 && ranges[0].endMeasure==10);
    assert(pcCountdownAlpha(4)==1 && pcCountdownAlpha(3)==1 && pcCountdownAlpha(2.5)==.5f && pcCountdownAlpha(2)==0 && pcCountdownAlpha(0)==0);
    assert(pcCountdownSeconds(.49)==0 && pcCountdownSeconds(.5)==1);
    assert(pcCountdownSeconds(1.5)==2 && pcCountdownSeconds(2.5)==3);
    assert(pcCountdownSeconds(3.5)==4 && pcCountdownSeconds(4.5)==5 && pcCountdownSeconds(10.5)==11);
    puts("PASS PC countdown gap threshold, sustain overlap, measures and final-note visibility fade");
}
