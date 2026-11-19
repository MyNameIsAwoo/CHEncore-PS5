// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/midi_drums.hpp"
#include "gameplay/midi_profile.hpp"
#include "gameplay/midi_setup.hpp"
#include <cassert>
#include <cstdio>
#include <unistd.h>
#include <vector>
using namespace rhythm::midi;
int main(){
    const uint8_t descriptors[]{
        9,4,0,0,1,1,2,0,0, 7,5,0x81,2,64,0,0, // audio, not MIDI
        9,4,1,0,2,1,3,0,0, 7,0x24,1,0,1,7,0,
        9,5,0x82,2,0,2,0,0,0, 9,5,2,2,0,2,0,0,0,
        9,4,2,0,1,1,3,0,0, 7,0x24,1,0,2,7,0, 7,5,0x83,2,64,0,0, // MIDI 2.0
        9,4,3,1,1,1,3,0,0, 7,0x24,1,0,1,7,0, 7,5,0x84,2,64,0,0 // inactive alt
    };
    std::array<UsbInputEndpoint,4> endpoints{};
    assert(inputEndpoints(descriptors,endpoints)==1);
    assert(endpoints[0].interface==1 && endpoints[0].address==0x82 && endpoints[0].packetBytes==512);
    assert(inputEndpoints(std::span(descriptors).first(sizeof(descriptors)-1),endpoints)==0);
    UsbDecoder decoder;
    std::vector<Strike> events;
    const auto receive=[&](const Strike& s){events.push_back(s);};
    const uint8_t stream[]{
        0x09,0x99,36,1, 0x09,0x99,36,127, // repeated kicks, no Note Off
        0x19,0x92,42,55, // another cable/channel
        0x08,0x89,36,0, 0x09,0x99,36,0, // release forms
        0x0f,0xf8,0,0, 0x04,0xf0,1,2, // clock and SysEx
        0x09,0x89,36,80, 0x09,0x99,128,80, 0x09,0x99,36,128 // malformed
    };
    decoder.feed({stream,3},100,receive);assert(events.empty());
    decoder.feed({stream+3,sizeof(stream)-3},101,receive);
    assert(events.size()==3 && events[0].velocity==1 && events[1].velocity==127);
    assert(events[2].cable==1 && events[2].channel==2 && events[2].note==42);
    // Endpoint/device queues never combine sources, even for identical notes.
    StrikeQueue<4> first,second;
    first.push(events[0]);first.push(events[0]);second.push(events[2]);
    unsigned accepted=0;assert(first.drain([&](auto s){assert(s==events[0]);++accepted;})==0);
    assert(accepted==2 && second.size()==1 && first.size()==0);
    for(unsigned i=0;i<6;++i)first.push({i,0,9,36,100});
    accepted=0;assert(first.drain([&](auto s){assert(s.receivedUs==accepted+2);++accepted;})==2);
    assert(accepted==4);
    Mapping map;
    assert(!map.bind({Action(255),0,9,36}));
    assert(!map.bind({Action::Red,0,9,38,10,{80,30}}));
    assert(map.bind({Action::Kick,0,9,36,127,{30,100}}));
    assert(map.bind({Action::Kick,0,9,35}));
    assert(map.bind({Action::Kick,0,9,36}));assert(map.bindings().size()==2);
    assert(map.bindings()[0].minimumVelocity==1 && !map.bindings()[0].velocity.calibrated());
    assert(!map.bind({Action::Red,0,9,36}));
    assert(map.bind({Action::Red,0,9,38,10,{30,110}}));
    std::vector<MappedStrike> hits;
    const auto hit=[&](auto s){hits.push_back(s);};
    for(unsigned i=0;i<200;++i)map.apply({uint64_t(i*500),0,9,uint8_t(i&1?35:36),uint8_t(i%127+1)},hit);
    assert(hits.size()==200);
    for(const auto& s:hits)assert(s.mask==1 && s.velocity==-1);
    map.apply({100001,0,9,38,30},hit);assert(hits.back().mask==2 && hits.back().velocity==0);
    map.apply({100002,0,9,38,110},hit);assert(hits.back().velocity==127);
    const auto count=hits.size();
    map.apply({100003,1,9,38,100},hit);map.apply({100003,0,8,38,100},hit);
    map.apply({100003,0,9,38,9},hit);map.apply({100003,0,9,36,0},hit);
    assert(hits.size()==count);
    Mapping softPad;
    assert(softPad.bind({Action::Red,0,9,38,10,{5,100}}));
    softPad.apply({100004,0,9,38,5},hit);
    assert(hits.size()==count+1 && hits.back().velocity==0);
    VelocityRange range;assert(!range.capture(50,50) && !range.capture(80,30));
    assert(!range.capture(0,127) && !range.capture(10,128));assert(range.capture(30,100));
    assert(range.normalize(1)==0 && range.normalize(127)==127);
    Mapping five(Layout::FiveLane);assert(!five.bind({Action::YellowPad,0,9,44}));
    assert(five.bind({Action::YellowCymbal,0,9,44}));
    assert(five.bind({Action::OrangeCymbal,0,9,45}));
    five.apply({0,0,9,44,100},hit);assert(hits.back().mask==32);
    five.apply({0,0,9,45,100},hit);assert(hits.back().mask==128);
    for(auto layout:{Layout::Pro4,Layout::FiveLane}){
        const auto one=setupOrder(layout,false),two=setupOrder(layout,true);
        assert(two.count==one.count+1 && two.actions[0]==Action::Kick && two.actions[1]==Action::Kick);
        const auto calibration=setupOrder(layout,true,true);
        for(unsigned i=0;i<calibration.count;++i)assert(calibration.actions[i]!=Action::Kick);
    }
    Profile profile;profile.vid=0x1234;profile.pid=0x5678;profile.name="USB Drum Kit";
    profile.twoKickPads=true;profile.mapping=map;
    const auto text=writeProfile(profile);Profile restored;std::string error;
    assert(parseProfile(text,restored,error));assert(restored.mapping.bindings().size()==3);
    assert(restored.vid==profile.vid && restored.pid==profile.pid && restored.twoKickPads);
    // Kick serialization has no velocity fields. Injecting calibration into a
    // kick is rejected, preserving the caller's previous profile on errors.
    const auto kick=text.find("\"kick\"");assert(kick!=std::string::npos);
    auto invalid=text;invalid.insert(kick+6,",\"velocity\":{\"soft\":20,\"hard\":90}");
    assert(!parseProfile(invalid,restored,error));assert(restored.name==profile.name);
    assert(!parseProfile(std::string(65537,'x'),restored,error));
    char directory[]="/tmp/encore-midi-XXXXXX";assert(mkdtemp(directory));
    const auto path=std::string(directory)+"/controller.midi.json";
    assert(saveProfile(path.c_str(),profile,error));
    assert(loadProfile(path.c_str(),restored,error));
    assert(restored.mapping.bindings()[2].velocity.calibrated());
    assert(restored.mapping.bindings()[2].velocity.soft==30 && restored.mapping.bindings()[2].velocity.hard==110);
    const auto alias=std::string(directory)+"/alias";assert(symlink(path.c_str(),alias.c_str())==0);
    assert(!loadProfile(alias.c_str(),restored,error));
    unlink(alias.c_str());unlink(path.c_str());rmdir(directory);
    SetupWizard wizard;wizard.begin(1,2,"Test Kit");
    assert(!wizard.selectKickPads(false,0));
    assert(wizard.selectLayout(Layout::Pro4) && wizard.selectKickPads(true,100));
    uint64_t at=100;unsigned note=35;
    while(wizard.page==SetupWizard::Page::Capture){
        assert(wizard.sample({at,0,9,uint8_t(note),100}));
        assert(!wizard.sample({at+1,0,9,uint8_t(note),100})); // setup transition bounce
        ++note;at+=300000;
    }
    assert(wizard.page==SetupWizard::Page::OfferVelocity);
    assert(wizard.chooseVelocity(true,at));
    unsigned calibrated=0;
    while(wizard.page!=SetupWizard::Page::Review){
        assert(wizard.action()!=Action::Kick);
        const auto list=wizard.draft.mapping.bindings();
        const auto found=std::find_if(list.begin(),list.end(),[&](auto b){return b.action==wizard.action();});
        assert(found!=list.end());
        assert(wizard.sample({at,found->cable,found->channel,found->note,30}));at+=300000;
        assert(!wizard.sample({at,found->cable,found->channel,found->note,20}));at+=300000;
        assert(wizard.sample({at,found->cable,found->channel,found->note,110}));at+=300000;
        ++calibrated;
    }
    assert(calibrated==7);
    for(auto b:wizard.draft.mapping.bindings())assert(b.action==Action::Kick?!b.velocity.calibrated():b.velocity.calibrated());
    puts("PASS USB MIDI events, malformed/release filtering, endpoint isolation, bounded queues, fast double kicks, layouts and pad-only velocity calibration");
}
