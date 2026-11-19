// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdio>
#include <cstring>
#include <cstdint>
namespace rhythm {
// Prepared on the library worker, never by a menu draw or Ready selection.
// Same PC section names and playable-lane rules as the gameplay adapter.
struct ChartCatalog {std::array<uint8_t,10> difficulties{};uint16_t instruments{};};
inline ChartCatalog chartCatalog(const char* path) noexcept {
    ChartCatalog result;FILE* file=std::fopen(path,"rb");if(!file)return result;
    constexpr const char* tracks[]{"Single","DoubleGuitar","DoubleRhythm","DoubleBass","Drums","Keyboard","GHLGuitar","GHLCoop","GHLRhythm","GHLBass"};
    constexpr const char* levels[]{"Easy","Medium","Hard","Expert"};
    char line[2048];int track=-1,difficulty=-1;size_t bytes=0;
    while(std::fgets(line,sizeof(line),file) && (bytes+=std::strlen(line))<=32*1024*1024) {
        const char* text=line+std::strspn(line," \t\r\n");
        if(*text=='[') {
            track=difficulty=-1;char name[80];
            if(std::sscanf(text,"[%79[^]]]",name)!=1)continue;
            for(int t=0;t<10;++t)for(int d=0;d<4;++d) {
                char expected[80];std::snprintf(expected,sizeof(expected),"%s%s",levels[d],tracks[t]);
                if(!std::strcmp(name,expected)){track=t;difficulty=d;}
            }
        } else if(track>=0 && !(result.difficulties[track]&(1u<<difficulty))) {
            long long tick,lane,length;
            if(std::sscanf(text,"%lld = N %lld %lld",&tick,&lane,&length)==3 && tick>=0 && length>=0 &&
               ((lane>=0 && lane<=4) || lane==7 || (track==4 && lane==5) || (track>=6 && lane==8))) {
                result.difficulties[track]|=1u<<difficulty;result.instruments|=1u<<track;
            }
        }
    }
    std::fclose(file);return result;
}
}
