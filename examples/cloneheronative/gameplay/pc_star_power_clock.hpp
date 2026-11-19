// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include "pc_beats.hpp"
#include <algorithm>
#include <vector>
#include <utility>
#include <cmath>
#include <cstdint>
namespace rhythm {
// BaseEngine 1820EFA90 -> 1820B8910: SP runs on a separate measure clock.
// Each measure spans 3.75*resolution SP ticks; full capacity is eight measures.
class PcStarPowerClock {
public:
    struct Segment {double time,tick,bpm,factor;};
    std::vector<Segment> segments;
    int resolution{192};
    template<class TickTime> void build(int ppq,const std::vector<std::pair<long,double>>& tempos,
        const std::vector<BeatSignature>& signatures,TickTime tickTime) {
        resolution=ppq;segments.clear();std::vector<long> boundaries{0};
        for(const auto& tempo:tempos)boundaries.push_back(tempo.first);
        for(const auto& signature:signatures)boundaries.push_back(signature.tick);
        std::sort(boundaries.begin(),boundaries.end());boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
        size_t tempo=0,signature=0;
        for(long raw:boundaries) {
            while(tempo+1<tempos.size() && tempos[tempo+1].first<=raw)++tempo;
            while(signature+1<signatures.size() && signatures[signature+1].tick<=raw)++signature;
            const double time=tickTime(raw);
            double origin=0;
            if(!segments.empty()) {const auto& old=segments.back();origin=old.tick+((time-old.time)*resolution*old.bpm)/(old.factor*60.0);}
            const auto& ts=signatures[signature];
            segments.push_back({time,origin,tempos[tempo].second,(double(ts.numerator)*4/ts.denominator)/3.75});
        }
    }
    [[nodiscard]] double position(double time) const noexcept {
        if(time<0 || segments.empty())return 0;
        auto at=std::upper_bound(segments.begin(),segments.end(),time,[](double t,const Segment& s){return t<s.time;});
        const auto& s=at==segments.begin()?segments.front():*(at-1);
        return ((time-s.time)*resolution*s.bpm)/(s.factor*60.0)+s.tick;
    }
    [[nodiscard]] double timeAt(double tick) const noexcept {
        if(segments.empty())return 0;
        auto at=std::upper_bound(segments.begin(),segments.end(),tick,[](double t,const Segment& s){return t<s.tick;});
        const auto& s=at==segments.begin()?segments.front():*(at-1);
        return (tick-s.tick)*(s.factor*60.0)/(resolution*s.bpm)+s.time;
    }
    // BaseEngine 0x1820EFB20 walks from the deployment's fractional
    // position, rather than inverting a truncated global tick. Integer charge
    // can reach zero just before the independently scheduled active deadline.
    [[nodiscard]] double afterTicks(double time,std::int64_t ticks) const noexcept {
        if(segments.empty())return time;
        auto at=std::upper_bound(segments.begin(),segments.end(),time,[](double t,const Segment& s){return t<s.time;});
        size_t index=at==segments.begin()?0:static_cast<size_t>(at-segments.begin()-1);
        const auto& start=segments[index];
        double offset=((time-start.time)*resolution*start.bpm)/(start.factor*60.0)+double(ticks);
        double result=start.time;
        for(;index<segments.size();++index) {
            const auto& segment=segments[index];
            const auto duration=[&](double value){return (value*60.0*segment.factor)/(resolution*segment.bpm);};
            if(index+1==segments.size())return result+duration(offset);
            const double width=segments[index+1].tick-segment.tick;
            if(offset<width)return result+duration(offset);
            offset-=width;result+=duration(width);
        }
        return result;
    }
};
}
