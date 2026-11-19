// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/guitar_session.hpp"
#include "gameplay/raw_input_queue.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace rhythm;
struct Sample {uint64_t us;GuitarButtons input;};
struct Result {std::vector<size_t> hits;std::vector<double> times;size_t misses;};
Result replay(const SongChart& chart,const std::vector<Sample>& samples,double fps,bool jitter) {
    GuitarSession game(chart);game.setHitWindow(.080);game.update({},0);
    Result result;size_t cursor=0;double previous=0;GuitarButtons held{};
    const auto update=[&](GuitarButtons state,double at) {
        game.update(state,at);
        for(const auto& e:game.events())if(e.kind==GuitarEventKind::Hit) {
            result.hits.push_back(e.noteIndex);result.times.push_back(e.time);
        }
    };
    // Independent render schedules consume identical timestamped USB reports,
    // including complete 2ms strums that begin and end between display frames.
    uint64_t now=0;unsigned frame=0;
    while(now<samples.back().us+500000) {
        now+=uint64_t(1000000/fps)*(jitter && ++frame%13==0?7:1);
        const double clock=double(now)/1000000;
        while(cursor<samples.size() && samples[cursor].us<=now) {
            const auto& sample=samples[cursor++];
            const double at=controls::rawInputSongTime(sample.us,now,clock,previous,1.0);
            held=sample.input;update(held,at);previous=at;
        }
        update(held,clock);previous=clock;
    }
    result.misses=game.misses();return result;
}
int main(int argc,char** argv) {
    const auto compare=[&](const SongChart& chart,const std::vector<Sample>& reports,const std::vector<size_t>& expected) {
        const auto reference=replay(chart,reports,1000,false);
        assert(reference.hits==expected && reference.misses==chart.notes.size()-expected.size());
        for(double fps:{30,60,120,240})for(bool jitter:{false,true}) {
            const auto actual=replay(chart,reports,fps,jitter);
            assert(actual.hits==reference.hits && actual.misses==reference.misses);
            assert(actual.times.size()==reference.times.size());
            for(size_t i=0;i<actual.times.size();++i)assert(std::abs(actual.times[i]-reference.times[i])<1e-12);
        }
    };
    for(bool legato:{false,true})for(double spacing:{.030,.040,.0625,.083333}) {
        SongChart chart;std::vector<Sample> reports;std::vector<size_t> expected;
        for(unsigned i=0;i<160;++i) {
            const uint64_t us=1000000+uint64_t(i*spacing*1000000);
            const uint16_t fret=1u<<(i%5);
            chart.notes.push_back({double(us)/1000000,0,fret,false,legato});
            if(i%9==4)continue; // deliberately miss a head inside a packed run
            expected.push_back(i);
            reports.push_back({us,{fret,bool(i%2),!bool(i%2)}});
            reports.push_back({us+2000,{0,false,false}});
        }
        const auto reference=replay(chart,reports,1000,false);
        assert(reference.hits==expected && reference.misses==160-expected.size());
        for(double fps:{30,60,120,240})for(bool jitter:{false,true}) {
            const auto actual=replay(chart,reports,fps,jitter);
            assert(actual.hits==reference.hits && actual.misses==reference.misses);
            assert(actual.times.size()==reference.times.size());
            for(size_t i=0;i<actual.times.size();++i)assert(std::abs(actual.times[i]-reference.times[i])<1e-12);
        }
    }
    // Repeated frets/chords and open runs must recover on the intended head,
    // rather than stay a note behind after an omitted strum.
    for(uint16_t mask:{0,1,16,3,21,31})for(double spacing:{.010,.030,.040,.075})
      for(bool alternate:{false,true}) {
        SongChart chart;std::vector<Sample> reports;std::vector<size_t> expected;
        for(unsigned i=0;i<160;++i) {
            const uint64_t us=1000000+uint64_t(i*spacing*1000000);
            chart.notes.push_back({double(us)/1000000,0,mask,false,false,std::popcount(mask)>1,mask==0});
            if(i%11==5)continue;
            expected.push_back(i);const bool up=alternate && i%2;
            reports.push_back({us,{mask,up,!up}});
            reports.push_back({us+2000,{mask,false,false}});
        }
        compare(chart,reports,expected);
    }
    for(double spacing:{.030,.040,.0625,.075,.083333}) {
        SongChart taps;std::vector<Sample> reports;std::vector<size_t> expected;
        for(unsigned i=0;i<160;++i) {
            const uint64_t us=1000000+uint64_t(i*spacing*1000000);
            const uint16_t fret=1u<<(i%5);
            taps.notes.push_back({double(us)/1000000,0,fret,true});
            if(i%9==4)continue;
            expected.push_back(i);reports.push_back({us,{fret,false,false}});
            reports.push_back({us+2000,{}});
        }
        compare(taps,reports,expected);
    }
    // Pull-offs count too: keep green anchored beneath higher tap frets.
    SongChart taps{{{1,0,1,true},{1.03,0,2,true},{1.06,0,4,true},
        {1.09,0,1,true},{1.12,0,8,true}}};
    compare(taps,{{1000000,{1,false,false}},{1060000,{5,false,false}},
        {1090000,{1,false,false}},{1120000,{9,false,false}}},{0,2,3,4});
    // No new fret event means no second same-fret tap, even with overlapping windows.
    SongChart repeated{{{1,0,1,true},{1.03,0,1,true}}};GuitarSession held(repeated);
    held.setHitWindow(.080);held.update({},0);held.update({1,false,false},1);
    held.update({1,false,false},1.06);assert(held.hits()==1);
    held.update({},1.061);held.update({1,false,false},1.062);assert(held.hits()==2);
    // A fresh press within the next early window recovers before the older
    // late deadline, with exactly one ordered miss followed by one hit.
    SongChart overlap{{{1,0,1,true},{1.075,0,2,true},{1.15,0,4,true}}};
    GuitarSession early(overlap);early.setHitWindow(.080);early.update({},0);
    early.update({1,false,false},1);early.update({},1.002);
    early.update({4,false,false},1.071);
    assert(early.hits()==2 && early.misses()==1 && early.events().size()==2);
    assert(early.events()[0].kind==GuitarEventKind::Miss && early.events()[1].kind==GuitarEventKind::Hit);
    GuitarSession allStrums(overlap);allStrums.setAllStrums(true);
    allStrums.update({},0);allStrums.update({4,false,false},1.15);assert(!allStrums.hits());
    // Optional private, PC-parsed tap run: no original song data in this test.
    if(argc==2) {
        FILE* f=std::fopen(argv[1],"r");assert(f);SongChart actual;unsigned count=0;
        assert(std::fscanf(f,"%u",&count)==1 && count>20);
        std::vector<Sample> reports;std::vector<size_t> expected;
        for(unsigned i=0;i<count;++i) {
            double time;unsigned fret;assert(std::fscanf(f,"%lf %u",&time,&fret)==2);
            actual.notes.push_back({time,0,static_cast<uint16_t>(fret),true});
            if(i%11==5)continue;
            const auto us=uint64_t(std::llround(time*1000000));
            expected.push_back(i);reports.push_back({us,{static_cast<uint16_t>(fret),false,false}});
            reports.push_back({us+2000,{}});
        }
        std::fclose(f);compare(actual,reports,expected);
        std::printf("PASS installed-chart tap run: %u heads with deliberate misses across all frame schedules\n",count);
    }
    // Missed HOPOs require a new strum, which must recover immediately even
    // while the missed head is still inside its own late window.
    SongChart recovery{{{1,0,1},{1.03,0,2,false,true},{1.06,0,4,false,true},{1.09,0,8,false,true}}};
    GuitarSession game(recovery);game.setHitWindow(.080);
    game.update({},0);game.update({1,false,true},1);game.update({},1.002);
    game.update({4,true,false},1.06);
    assert(game.hits()==2 && game.misses()==1);
    game.update({},1.062);game.update({8,false,true},1.09);
    assert(game.hits()==3 && game.misses()==1);
    // Fast alternating edges bypass ONLY same-direction debounce. A held
    // strum or an unchanged repeated sample never awards another head.
    SongChart rapid{{{1,0,1},{1.003,0,1},{1.006,0,1}}};
    GuitarSession alternate(rapid);alternate.setHitWindow(.080);alternate.setStrumDebounce(.050);
    alternate.update({},0);alternate.update({1,false,true},1);
    alternate.update({1,true,false},1.003);alternate.update({1,false,true},1.006);
    assert(alternate.hits()==3 && !alternate.overstrums());
    GuitarSession heldStrum(rapid);heldStrum.setHitWindow(.080);
    heldStrum.update({},0);heldStrum.update({1,false,true},1);
    heldStrum.update({1,false,true},1.003);heldStrum.update({1,false,true},1.006);
    heldStrum.update({1,false,true},1.2);assert(heldStrum.hits()==1 && heldStrum.misses()==2);
    GuitarSession bounce(rapid);bounce.setHitWindow(.080);
    bounce.update({},0);bounce.update({1,false,true},1);bounce.update({1,false,false},1.001);
    bounce.update({1,false,true},1.002);assert(bounce.hits()==1);
    SongChart wrong{{{1,0,1},{1.03,0,2},{1.06,0,4}}};GuitarSession reject(wrong);
    reject.setHitWindow(.080);reject.update({},0);reject.update({1,false,true},1);
    reject.update({16,true,false},1.03);reject.update({16,false,true},1.06);
    reject.update({},1.3);assert(reject.hits()==1 && reject.misses()==2);
    // Late hits just inside the widened window survive all render schedules.
    for(double late:{.071,.075,.079}) {
        SongChart single{{{1,0,1}}};const auto us=uint64_t((1+late)*1000000);
        std::vector<Sample> reports{{us,{1,false,true}},{us+2000,{}}};
        for(double fps:{30,60,120,240}) {
            const auto result=replay(single,reports,fps,true);assert(result.hits.size()==1 && !result.misses);
        }
    }
    SongChart single{{{1,0,1}}};GuitarSession outside(single);outside.setHitWindow(.080);
    outside.update({},0);outside.update({1,false,true},1.081);assert(!outside.hits() && outside.misses()==1);
    std::puts("PASS 30/60/120/240fps and stalled frames: dense runs, 2ms input pulses, immediate miss recovery, timestamps and window boundaries");
}
