// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include "controller_profile.hpp"
namespace rhythm::controls::xbox360 {
// Wire facts: Linux xpad and InvoxiPlayGames/x360-research Argon protocol.
inline constexpr std::array<uint8_t,12> presence{{8,0,15,192,0,0,0,0,0,0,0,0}};
inline constexpr std::array<uint8_t,12> capabilities{{0,0,2,128,0,0,0,0,0,0,0,0}};
inline bool receiver(uint16_t vid,uint16_t pid) noexcept {return vid==0x045e && (pid==0x0719 || pid==0x0291 || pid==0x02a9);}
inline int wiredType(uint16_t vid,uint16_t pid) noexcept {
    if(vid==0x1430 && pid==0x4748)return 7; // GH X-plorer
    if(vid==0x1bad && pid==0x0002)return 6; // RB wired Stratocaster
    if(vid==0x1bad && (pid==0x0003 || pid==0x0130))return 8; // RB / ION drums
    return -1;
}
struct Channel {
    bool connected{};unsigned subtype{};
    enum class Result {Ignore,Disconnect,Connect,Identity,Input};
    Result consume(const uint8_t* data,size_t size,std::array<uint8_t,64>& report) noexcept {
        if(!data || size<2)return Result::Ignore;
        if(data[0]&8) {
            const bool next=(data[1]&128)!=0;
            if(!next){connected=false;subtype=0;report.fill(0);return Result::Disconnect;}
            connected=true;return Result::Connect;
        }
        if(data[1]==15 && size>=26 && data[5]==0xcc){connected=true;subtype=data[25];return Result::Identity;}
        if(data[1]!=1 || size<24 || data[4]!=0 || data[5]<19)return Result::Ignore;
        connected=true;std::copy_n(data+4,20,report.begin());report[1]=20;return Result::Input;
    }
};
inline bool wiredReport(const uint8_t* data,size_t size) noexcept {return data && size>=20 && data[0]==0 && data[1]>=20;}
inline ControllerProfile profile(uint16_t vid,uint16_t pid,unsigned subtype,int instrument=0) {
    ControllerProfile p;p.schema=2;p.vid=vid;p.pid=pid;p.source=Source::RawHid;p.reportLength=20;p.reportId=0;
    p.instrument=instrumentName(instrument);p.name=subtype==6?"Xbox 360 Rock Band guitar":subtype==7?"Xbox 360 Guitar Hero guitar":"Xbox 360 drums";
    const auto digital=[](int byte,uint32_t mask){Locator l;l.byte=byte;l.mask=mask;return l;};
    const auto add=[&](const char* action,int byte,uint32_t mask){Binding b;b.action=action;b.input=digital(byte,mask);p.bindings.push_back(std::move(b));};
    if(instrument==0) {
        add("green",3,16);add("red",3,32);add("yellow",3,subtype==6?64:128);add("blue",3,subtype==6?128:64);add("orange",3,1);
        add("strum_up",2,1);add("strum_down",2,2);
    } else {
        add("kick",3,1);p.bindings.back().alternatives.push_back(digital(2,64));
        add("pad_red",3,32);add("pad_yellow",3,128);add("pad_blue",3,64);add("pad_green",3,16);
        if(instrument==3)add("pad_orange",3,2);
    }
    add("start",2,16);add("star_power",2,32);return p;
}
}
