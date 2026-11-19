// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/controller_profile.hpp"
#include "gameplay/controller_navigation.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
int main(){
    using namespace rhythm::controls;std::string error;
    unsigned count=0;
    for(const auto& path:std::filesystem::directory_iterator("assets/controller_profiles")) {
        if(path.path().extension()!=".json" || path.path().filename()=="catalog.json")continue;
        ControllerProfile p,copy;assert(loadProfile(path.path().c_str(),p,error));
        assert(parseProfile(writeProfile(p),copy,error));assert(copy.schema==p.schema && copy.reportId==p.reportId && copy.menuHat==p.menuHat);
        assert(copy.bindings.size()==p.bindings.size() && copy.menuBindings.size()==p.menuBindings.size() && copy.suppressHat==p.suppressHat);++count;
    }
    assert(count>=22);
    for(auto id:{"024a","0249"}) {
        const bool ps5=id==std::string("0249");ControllerProfile p;
        assert(loadProfile((std::string("assets/controller_profiles/0e6f_")+id+".json").c_str(),p,error));
        const int hatbyte=ps5?8:5,frets=ps5?43:46;
        std::array<uint8_t,64> raw{};raw[0]=1;raw[1]=raw[2]=128;raw[hatbyte]=ps5?15:8;
        assert(!applyProfile(p,{},raw.data(),raw.size()).frets);
        for(unsigned fret=0;fret<5;++fret)for(unsigned lower=0;lower<2;++lower) {
            raw[frets+lower]=1<<fret;auto state=applyProfile(p,{},raw.data(),raw.size());
            assert(state.frets==(1<<fret));raw[frets+lower]=0;
        }
        ReportCapture capture;capture.begin(raw.data(),raw.size());
        raw[frets]=1;raw[hatbyte]|=32;raw[12]=9;raw[13]=4;
        Locator captured;assert(capture.capture(raw.data(),raw.size(),false,captured,error,&p));
        assert(captured.byte==frets && captured.mask==1); // mirror flags/counters ignored
        raw[hatbyte]=(raw[hatbyte]&240)|0;raw[12]++;
        assert(capture.capture(raw.data(),raw.size(),true,captured,error,&p));
        assert(captured.byte==hatbyte && captured.mask==15 && captured.value==0);
        auto state=applyProfile(p,{},raw.data(),raw.size());assert(state.strumUp && state.frets==1);
        assert(rawMenuHat(p,raw.data(),raw.size())&MenuHatUp);
        raw[hatbyte]=(raw[hatbyte]&240)|4;assert(applyProfile(p,{},raw.data(),raw.size()).strumDown);
        raw[0]=2;assert(!applyProfile(p,{},raw.data(),raw.size()).frets); // unrelated report ignored
    }
    // RCM Wii adapters in PS3 GH output mode share the 27-byte GH report.
    // Exercise the shipped profile, independent of user gameplay remaps.
    ControllerProfile rcm;assert(loadProfile("assets/controller_profiles/12ba_0100.json",rcm,error));
    uint8_t ps3[27]{};ps3[2]=31; // Original PS3 neutral; some firmware uses 8.
    assert(!applyProfile(rcm,{},ps3,27).frets && !rawMenuHat(rcm,ps3,27));
    constexpr uint8_t ps3frets[]{2,4,1,8,16};
    for(unsigned i=0;i<5;++i) {
        ps3[0]=ps3frets[i];assert(applyProfile(rcm,{},ps3,27).frets==(1u<<i));
        assert(rawMenuControls(rcm,ps3,27).colors==(1u<<i));
    }
    ps3[0]=31;assert(applyProfile(rcm,{},ps3,27).frets==31);ps3[0]=0;
    for(unsigned neutral:{8u,31u}) {ps3[2]=neutral;assert(!applyProfile(rcm,{},ps3,27).strumUp && !applyProfile(rcm,{},ps3,27).strumDown);}
    for(unsigned i=0;i<40;++i) {ps3[2]=i%2?4:0;auto state=applyProfile(rcm,{},ps3,27);assert(state.strumUp==!(i%2) && state.strumDown==bool(i%2));}
    ps3[2]=31;ps3[1]=2;assert(applyProfile(rcm,{},ps3,27).start && rawMenuControls(rcm,ps3,27).start);
    ps3[1]=1;assert(applyProfile(rcm,{},ps3,27).starPower && rawMenuControls(rcm,ps3,27).select);
    auto remapped=rcm;remapped.schema=1;remapped.menuBindings.clear();remapped.menuHat=-1;
    remapped.bindings[0].input.mask=4;inheritLegacyMenuControls(remapped,rcm);
    ps3[1]=0;ps3[0]=2;assert(rawMenuControls(remapped,ps3,27).colors==1);
    assert(!(applyProfile(remapped,{},ps3,27).frets&1));
    assert(!applyProfile(rcm,{},ps3,26).frets && rawMenuControls(rcm,ps3,26)==RawMenuState{});
    // Santroller-mode revisions have a different layout: never use PS3 offsets.
    for(auto variant:{"0300","0400"}) {
        ControllerProfile santroller;assert(loadProfile((std::string("assets/controller_profiles/1209_2882_")+variant+".json").c_str(),santroller,error));
        uint8_t bytes[8]{};bytes[0]=1;bytes[3]=8;
        for(unsigned i=0;i<5;++i){bytes[1]=1u<<i;assert(applyProfile(santroller,{},bytes,santroller.reportLength).frets==(1u<<i));}
        bytes[0]=2;assert(!applyProfile(santroller,{},bytes,santroller.reportLength).frets);
    }
    ControllerProfile drums;assert(loadProfile("assets/controller_profiles/0738_8262.json",drums,error));
    std::array<uint8_t,64> raw{};raw[0]=1;raw[5]=8;raw[43]=1;
    auto state=applyProfile(drums,{},raw.data(),raw.size());assert(state.pads==2 && state.velocities[1]>0);
    raw[47]=127;raw[6]=2;state=applyProfile(drums,{},raw.data(),raw.size());assert((state.pads&35)==35);
    // Dedicated RB4 face buttons report no drum velocity. They must work
    // in menus without ever producing a gameplay hit or hiding in history.
    raw={};raw[0]=1;raw[5]=8;
    constexpr unsigned faceMask[]{32,64,128,16};
    for(unsigned color=0;color<4;++color) {
        auto pressed=raw;pressed[5]|=faceMask[color];
        assert(applyProfile(drums,{},pressed.data(),64).pads==0);
        assert(rawMenuControls(drums,pressed.data(),64).colors==(1u<<color));
        assert(!sameControllerInput(drums,raw.data(),64,pressed.data(),64));
        auto counter=pressed;counter[12]++;
        assert(sameControllerInput(drums,pressed.data(),64,counter.data(),64));
    }
    auto wrong=raw;wrong[0]=2;wrong[5]|=32;
    assert(rawMenuControls(drums,wrong.data(),64)==RawMenuState{});
    assert(rawMenuControls(drums,wrong.data(),63)==RawMenuState{});
    for(unsigned direction=0;direction<8;++direction) {
        auto pressed=raw;pressed[5]=direction|32;
        assert(rawMenuControls(drums,pressed.data(),64).colors==1);
        assert(rawMenuControls(drums,pressed.data(),64).directions!=0);
    }
    // Existing saved gameplay remaps inherit missing hardware menu metadata.
    auto legacy=drums;legacy.schema=2;legacy.menuBindings.clear();
    legacy.bindings[1].input.byte=42;
    const auto before=applyProfile(legacy,{},raw.data(),64);
    inheritLegacyMenuControls(legacy,drums);
    assert(legacy.schema==3 && legacy.menuBindings.size()==drums.menuBindings.size());
    assert(legacy.bindings[1].input.byte==42 && applyProfile(legacy,{},raw.data(),64)==before);
    auto explicitMenu=legacy;explicitMenu.menuBindings.clear();inheritLegacyMenuControls(explicitMenu,drums);
    assert(explicitMenu.menuBindings.empty());
    ControllerProfile copy;assert(parseProfile(writeProfile(legacy),copy,error));
    auto invalid=legacy;invalid.schema=2;assert(!parseProfile(writeProfile(invalid),copy,error));
    invalid=legacy;invalid.menuBindings[0].input.byte=64;assert(!parseProfile(writeProfile(invalid),copy,error));
    invalid=legacy;invalid.menuBindings.push_back(invalid.menuBindings[0]);assert(!parseProfile(writeProfile(invalid),copy,error));
    // PS3/Wii cymbals multiplex hat up/down. Those tags are not menu moves.
    for(auto id:{"12ba_0210","1bad_0005","1bad_3110"}) {
        ControllerProfile older;assert(loadProfile((std::string("assets/controller_profiles/")+id+".json").c_str(),older,error));
        uint8_t report[27]{};report[2]=8;
        for(unsigned direction:{0u,4u}) {
            report[2]=direction;report[1]=0;
            assert(rawMenuControls(older,report,27).directions);
            report[1]=8;assert(!rawMenuControls(older,report,27).directions);
            report[0]=8;assert(rawMenuControls(older,report,27).colors==4);
            report[0]=0;
        }
    }
    ControllerProfile five;assert(loadProfile("assets/controller_profiles/12ba_0120.json",five,error));
    uint8_t gh[27]{};gh[2]=8;gh[0]=32;
    assert(rawMenuControls(five,gh,27).colors==16);
    gh[0]=0;gh[2]=2;assert(rawMenuControls(five,gh,27).directions==MenuHatRight);
    std::puts("PASS all bundled profiles parse/roundtrip; both Riffmaster reports, lower frets, masked hats, safe remap capture, IDs and independent drum velocities/kicks");
}
