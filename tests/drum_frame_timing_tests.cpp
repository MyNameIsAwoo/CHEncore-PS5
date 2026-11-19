// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/drum_session.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace rhythm;
struct Strike {double time;uint16_t pads;};
struct Result {std::vector<size_t> hits;size_t misses;};
Result replay(const SongChart& chart,const std::vector<Strike>& strikes,double fps,bool pro,bool jitter) {
    DrumSession game(chart);game.setOptions(pro,false,false,false,100);
    pc_drums::PadEdges edges;Result result;size_t cursor=0;unsigned frame=0;
    const auto update=[&](DrumInput input,double at) {
        game.update(input,at);
        for(const auto& e:game.events())if(e.kind==DrumEventKind::Hit)result.hits.push_back(e.noteIndex);
    };
    double time=0;
    while(time<strikes.back().time+.3) {
        time+=1/fps*(jitter && ++frame%13==0?7:1);
        while(cursor<strikes.size() && strikes[cursor].time<=time) {
            const auto s=strikes[cursor++];DrumInput input;input.strikes=edges.sample(s.pads);
            update(input,s.time);
        }
        update({},time);
    }
    result.misses=game.misses();return result;
}
int main() {
    for(bool pro:{false,true})for(bool repeated:{false,true})for(double spacing:{.010,.030,.040,.075}) {
        SongChart chart;chart.kind=ChartKind::Drums;
        std::vector<Strike> strikes;std::vector<size_t> expected;
        for(unsigned i=0;i<160;++i) {
            const double time=1+i*spacing;
            const uint16_t pad=1u<<(repeated?1:i%(pro?8:5));
            chart.notes.push_back({time,0,pad});
            if(i%11==5)continue;
            expected.push_back(i);strikes.push_back({time,pad});strikes.push_back({time+.002,0});
        }
        const auto reference=replay(chart,strikes,1000,pro,false);
        assert(reference.hits==expected && reference.misses==chart.notes.size()-expected.size());
        for(double fps:{30,60,120,240})for(bool jitter:{false,true}) {
            const auto actual=replay(chart,strikes,fps,pro,jitter);
            assert(actual.hits==reference.hits && actual.misses==reference.misses);
        }
    }
    // Held pad is not a repeated strike; independent cymbal and kick events survive.
    pc_drums::PadEdges edges;assert(edges.sample(2)==2);assert(edges.sample(2)==0);
    assert(edges.sample(35)==33);assert(edges.sample(35)==0);
    SongChart chord;chord.kind=ChartKind::Drums;
    chord.notes={{1,0,1},{1,0,2},{1,0,32},{1.03,0,1},{1.03,0,2},{1.03,0,32}};
    DrumSession band(chord);band.setOptions(true,false,false,false,100);
    pc_drums::PadEdges held;DrumInput input;input.strikes=held.sample(35);band.update(input,1);
    assert(band.hits()==3);input.strikes=held.sample(35);band.update(input,1.03);
    assert(band.hits()==3);band.update({},1.3);assert(band.misses()==3);
    std::puts("PASS normal/pro drums: fast repeated pads, alternating pads/cymbals, kick and miss recovery across frame rates");
}
