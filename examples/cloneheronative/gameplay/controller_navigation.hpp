// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include "controller_profile.hpp"
namespace rhythm::controls {
enum MenuHatDirection : uint8_t {MenuHatUp=1,MenuHatDown=2,MenuHatLeft=4,MenuHatRight=8};
struct RawMenuState {
    uint8_t colors{},directions{};bool start{},select{};
    bool operator==(const RawMenuState&) const noexcept = default;
};
inline bool validRawMenuReport(const ControllerProfile& profile,const uint8_t* report,unsigned size) noexcept {
    return profile.source==Source::RawHid && report &&
        (!profile.reportLength || size==profile.reportLength) &&
        (profile.reportId<0 || (size && report[0]==profile.reportId));
}
inline bool rawMenuDigital(const Locator& input,const uint8_t* report,unsigned size) noexcept {
    return report && input.kind==Kind::Digital && input.byte>=0 && unsigned(input.byte)<size &&
        input.mask && (report[input.byte]&input.mask)==input.mask;
}
// A verified zero-based eight-way hat is identified by its existing Up=0 and
// Down=4 bindings on one byte. Other report layouts need their own device map.
// This only decodes menu navigation; it never changes gameplay strum bindings.
inline uint8_t rawMenuHat(const ControllerProfile& profile,const uint8_t* report,unsigned size) noexcept {
    if(!validRawMenuReport(profile,report,size))return 0;
    // Older RB cymbals reuse the hat bits for yellow/blue identification.
    if(profile.suppressHat && rawMenuDigital(profile.hatSuppression,report,size))return 0;
    const Locator *up=nullptr,*down=nullptr;
    for(const auto& binding:profile.bindings) {
        if(binding.action=="strum_up")up=&binding.input;
        if(binding.action=="strum_down")down=&binding.input;
    }
    if(profile.menuHat>=0 && unsigned(profile.menuHat)<size) {
        constexpr uint8_t directions[]{MenuHatUp,MenuHatUp|MenuHatRight,MenuHatRight,MenuHatDown|MenuHatRight,MenuHatDown,MenuHatDown|MenuHatLeft,MenuHatLeft,MenuHatUp|MenuHatLeft};
        const unsigned value=report[profile.menuHat]&profile.menuHatMask;return value<8?directions[value]:0;
    }
    if(up && down && up->kind==Kind::Digital && down->kind==Kind::Digital &&
       up->byte==down->byte && up->mask==1 && down->mask==2 && unsigned(up->byte)<size) {
        const unsigned buttons=report[up->byte];return ((buttons&1)?MenuHatUp:0)|((buttons&2)?MenuHatDown:0)|((buttons&4)?MenuHatLeft:0)|((buttons&8)?MenuHatRight:0);
    }
    if(!up || !down || up->kind!=Kind::Hat || down->kind!=Kind::Hat ||
       up->byte!=down->byte || up->byte<0 || unsigned(up->byte)>=size || up->value!=0 || down->value!=4)return 0;
    constexpr uint8_t directions[]{MenuHatUp,MenuHatUp|MenuHatRight,MenuHatRight,MenuHatDown|MenuHatRight,
        MenuHatDown,MenuHatDown|MenuHatLeft,MenuHatLeft,MenuHatUp|MenuHatLeft,0};
    const unsigned value=report[up->byte]&(up->mask?up->mask:255);return value<sizeof(directions)?directions[value]:0;
}
inline RawMenuState rawMenuControls(const ControllerProfile& profile,const uint8_t* report,unsigned size) noexcept {
    RawMenuState state;if(!validRawMenuReport(profile,report,size))return state;
    state.directions=rawMenuHat(profile,report,size);
    constexpr const char* colors[]{"green","red","yellow","blue","orange"};
    for(const auto& binding:profile.menuBindings) {
        bool pressed=rawMenuDigital(binding.input,report,size);
        for(const auto& alternative:binding.alternatives)pressed|=rawMenuDigital(alternative,report,size);
        if(!pressed)continue;
        for(unsigned i=0;i<5;++i)if(binding.action==colors[i])state.colors|=1u<<i;
        if(binding.action=="start")state.start=true;
        else if(binding.action=="select")state.select=true;
        else if(binding.action=="up")state.directions|=MenuHatUp;
        else if(binding.action=="down")state.directions|=MenuHatDown;
        else if(binding.action=="left")state.directions|=MenuHatLeft;
        else if(binding.action=="right")state.directions|=MenuHatRight;
    }
    return state;
}

// Upgrade only legacy device overrides. Preserve gameplay bindings and any
// explicitly authored schema-3 menu layout. The bundled fallback is cached.
inline void inheritLegacyMenuControls(ControllerProfile& custom,const ControllerProfile& hardware) {
    if(hardware.schema<3 || (hardware.menuBindings.empty() && !hardware.suppressHat) || custom.schema>=3 || custom.source!=Source::RawHid || hardware.source!=Source::RawHid ||
        custom.vid!=hardware.vid || custom.pid!=hardware.pid || custom.reportLength!=hardware.reportLength ||
        (custom.reportId>=0 && hardware.reportId>=0 && custom.reportId!=hardware.reportId))return;
    custom.menuBindings=hardware.menuBindings;
    if(custom.menuHat<0){custom.menuHat=hardware.menuHat;custom.menuHatMask=hardware.menuHatMask;}
    if(custom.menuHat==hardware.menuHat && hardware.suppressHat) {
        custom.hatSuppression=hardware.hatSuppression;custom.suppressHat=true;
    }
    if(!custom.menuBindings.empty() || custom.suppressHat)custom.schema=3;
}

}
