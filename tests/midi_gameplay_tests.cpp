// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/midi_gameplay.hpp"
#include <cassert>
#include <memory>
#include <cstdio>
using namespace rhythm;
using namespace rhythm::midi;
static void rapidKicks(unsigned fps){
    SongChart chart;chart.kind=ChartKind::Drums;
    for(unsigned i=0;i<128;++i)chart.notes.push_back({1.+i*.025,0,1});
    DrumSession session(chart);session.setOptions(true,false,false,false,100);
    Mapping mapping;assert(mapping.bind({Action::Kick,0,9,35}));assert(mapping.bind({Action::Kick,0,9,36}));
    UsbDecoder decoder;unsigned next=0,points=0;
    for(unsigned frame=0;frame<fps*5;++frame){
        const double end=double(frame)/fps;
        while(next<chart.notes.size() && chart.notes[next].time<=end){
            const auto at=chart.notes[next].time;
            const uint8_t bytes[]{9,0x99,uint8_t(next%2?35:36),uint8_t(1+next%127)};
            decoder.feed(bytes,uint64_t(at*1000000),[&](const Strike& hit){mapping.apply(hit,[&](const MappedStrike& event){
                const auto input=gameplayInput(event);
                assert(input.strikes==1 && input.velocities==DrumInput{}.velocities);
                session.update(input,at,{},[&](const DrumEvent& e){if(e.kind==DrumEventKind::Hit)points+=e.points;});
            });});++next;
        }
        session.update({},end);
    }
    assert(session.hits()==128 && session.misses()==0 && session.streak()==128 && points==128*50);
}
int main(){
    rapidKicks(30);rapidKicks(60);rapidKicks(120);
    // Four separate kits may use identical pitches. Cable/channel filtering
    // happens before judgement, and the same timestamp can include a chord.
    std::array<SongChart,4> charts;std::array<std::unique_ptr<DrumSession>,4> sessions;
    std::array<Mapping,4> mappings;
    for(unsigned i=0;i<4;++i){
        charts[i].kind=ChartKind::Drums;charts[i].notes={{1,0,2},{1,0,32},{1.5,0,64},{2,0,128}};
        sessions[i]=std::make_unique<DrumSession>(charts[i]);sessions[i]->setOptions(true,false,false,false,100);
        assert(mappings[i].bind({Action::Red,0,uint8_t(i),38}));
        assert(mappings[i].bind({Action::YellowCymbal,0,uint8_t(i),42}));
        assert(mappings[i].bind({Action::BlueCymbal,0,uint8_t(i),51}));
        assert(mappings[i].bind({Action::GreenCymbal,0,uint8_t(i),49}));
    }
    for(unsigned source=0;source<4;++source){
        for(auto [time,note]:{std::pair{1.,38},std::pair{1.,42},std::pair{1.5,51},std::pair{2.,49}}){
            mappings[source].apply({uint64_t(time*1000000),0,uint8_t(source),uint8_t(note),100},[&](auto event){sessions[source]->update(gameplayInput(event),time);});
        }
        assert(sessions[source]->hits()==4 && sessions[source]->misses()==0);
        for(unsigned other=source+1;other<4;++other)assert(sessions[other]->hits()==0);
    }
    assert(gameplayInput({0,3,100}).strikes==0);assert(gameplayInput({0,256,100}).strikes==0);
    Mapping five(Layout::FiveLane);assert(five.bind({Action::OrangeCymbal,0,9,49}));
    five.apply({0,0,9,49,127},[](auto event){assert(gameplayInput(event).strikes==128);});
    puts("PASS MIDI-to-drum judgement: 30/60/120 FPS, 128 alternating kicks, same-frame chords, cymbals and four isolated kits");
}
