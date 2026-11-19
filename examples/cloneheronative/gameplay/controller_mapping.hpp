// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace rhythm::controls {
// ScePad button bits. Options and the hat are reserved for navigation/escape.
inline constexpr std::array<uint32_t,14> buttons{0x4000,0x2000,0x8000,0x1000,0x400,0x800,0x100,0x200,0x100000,0x10,0x40,0x8,0x20,0x80};
inline const char* buttonName(uint32_t button) noexcept {
    constexpr const char* names[]{"Cross","Circle","Square","Triangle","L1","R1","L2","R2","Touchpad","D-pad Up","D-pad Down","Options","D-pad Right","D-pad Left"};
    for(unsigned i=0;i<buttons.size();++i) if(buttons[i]==button)return names[i];
    return "Unassigned";
}
inline unsigned count(int type) noexcept {return type==0?9:type==1?10:type==2?7:type==3?8:10;}
inline const char* action(int type,unsigned index) noexcept {
    constexpr const char* guitar[]{"Green","Red","Yellow","Blue","Orange","Strum Up","Strum Down","Start","Star Power"};
    constexpr const char* six[]{"White 1","White 2","White 3","Black 1","Black 2","Black 3","Strum Up","Strum Down","Start","Star Power"};
    constexpr const char* drums[]{"Kick Pedal","Red Pad","Yellow Pad / Cymbal","Blue Pad","Green Pad","Start","Star Power"};
    constexpr const char* five[]{"Kick Pedal","Green Pad","Red Pad","Yellow Cymbal","Blue Pad","Orange Cymbal","Start","Star Power"};
    constexpr const char* pro[]{"Kick Pedal","Red Pad","Yellow Pad","Yellow Cymbal","Blue Pad","Blue Cymbal","Green Pad","Green Cymbal","Start","Star Power"};
    return type==4?pro[index]:type==1?six[index]:type==2?drums[index]:type==3?five[index]:guitar[index];
}
inline unsigned greenIndex(int type) noexcept {return type==3?1:type==4?6:type==2?4:0;}
inline unsigned redIndex(int type) noexcept {return type==3?2:1;}
struct Mapping {std::array<uint32_t,10> keys{};bool custom{false};};
inline Mapping defaults(int type) noexcept {
    Mapping m;
    if(type==0)m.keys={0x4000,0x2000,0x8000,0x1000,0x400,0x10,0x40,0x8,0x100000,0};
    else if(type==1)m.keys={0x8000,0x400,0x800,0x4000,0x2000,0x1000,0x10,0x40,0x8,0x100000};
    else if(type==2)m.keys={0x400,0x2000,0x1000,0x8000,0x4000,0x8,0x100000,0,0,0};
    else if(type==3)m.keys={0x400,0x4000,0x2000,0x1000,0x8000,0x800,0x8,0x100000,0,0};
    else m.keys={0x400,0x2000,0x1000,0x800,0x8000,0x100,0x4000,0x200,0x8,0x100000};
    return m;
}
inline bool valid(const Mapping& m,int type) noexcept {
    if(type<0 || type>4)return false;
    for(unsigned i=0;i<count(type);++i) {
        if(std::find(buttons.begin(),buttons.end(),m.keys[i])==buttons.end())return false;
        for(unsigned j=0;j<i;++j)if(m.keys[i]==m.keys[j])return false;
    }
    return true;
}
struct Remap {
    enum class Page {Capture,Conflict,Review};
    Mapping draft{};int type{},row{};unsigned index{},duplicate{};uint32_t candidate{};
    Page page{Page::Capture};
    void begin(int instrument,const Mapping& current) noexcept {
        type=instrument;draft=current;index=0;row=0;candidate=0;page=Page::Capture;
    }
    void advance() noexcept {if(++index==count(type)){page=Page::Review;row=0;}}
    void capture(uint32_t button) noexcept {
        if(page!=Page::Capture || std::find(buttons.begin(),buttons.end(),button)==buttons.end())return;
        const bool strum=type<2 && (index==count(type)-4 || index==count(type)-3);
        if((button&0xf0u) && !strum)return;
        if(button==0x8u && index!=count(type)-2)return;
        for(unsigned i=0;i<count(type);++i) if(i!=index && draft.keys[i]==button) {
            candidate=button;duplicate=i;page=Page::Conflict;return;
        }
        draft.keys[index]=button;advance();
    }
    void swap() noexcept {
        if(page!=Page::Conflict)return;
        std::swap(draft.keys[index],draft.keys[duplicate]);page=Page::Capture;advance();
    }
    void retryConflict() noexcept {page=Page::Capture;candidate=0;}
};
}
