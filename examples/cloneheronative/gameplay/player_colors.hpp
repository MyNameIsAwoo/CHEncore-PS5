// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace rhythm {
inline constexpr std::array<const char*,10> noteColorNames{{"DEFAULT","GREEN","RED","YELLOW","BLUE","ORANGE","PURPLE","SILVER","GOLD","RGB"}};
inline std::array<float,3> regularNoteColor(int choice,double songTime) noexcept {
    static constexpr std::array<std::array<float,3>,9> colors{{
        {{1,1,1}},{{0,1,0}},{{1,0,0}},{{1,1,0}},{{0,0,1}},{{1,.5f,0}},{{.68f,.32f,1}},{{.78f,.82f,.88f}},{{1,.72f,.12f}}
    }};
    if(choice!=9)return colors[std::clamp(choice,0,8)];
    double phase=std::fmod(songTime/8.0,1.0);if(phase<0)phase+=1;
    const float h=float(phase*6),f=h-std::floor(h);
    switch(int(h)) {
        case 0:return {1,f,0};case 1:return {1-f,1,0};case 2:return {0,1,f};
        case 3:return {0,1-f,1};case 4:return {f,0,1};default:return {1,0,1-f};
    }
}

enum class HighwayPulse { None, FiveStars, Streak, Solo, Danger };
inline HighwayPulse highwayPulse(bool enabled,float health,bool solo,unsigned streak,unsigned stars) noexcept {
    if(!enabled)return HighwayPulse::None;
    if(health<=1.f/3)return HighwayPulse::Danger;
    if(solo)return HighwayPulse::Solo;
    if(streak>=30)return HighwayPulse::Streak;
    if(stars>=5)return HighwayPulse::FiveStars;
    return HighwayPulse::None;
}
inline std::array<float,3> highwayPulseTint(HighwayPulse kind,double clock) noexcept {
    if(kind==HighwayPulse::None)return {1,1,1};
    if(kind==HighwayPulse::Solo)return {.22f,.42f,1.0f};
    const float pulse=.5f+.5f*std::sin(float(std::fmod(clock,8.0))*6.28318530718f*(kind==HighwayPulse::Danger?1.5f:.65f));
    const float depth=(kind==HighwayPulse::FiveStars?.16f:.48f)*pulse;
    const auto hue=kind==HighwayPulse::Danger?std::array<float,3>{.65f,.04f,.04f}:
        kind==HighwayPulse::Solo?std::array<float,3>{.04f,.08f,.65f}:
        kind==HighwayPulse::Streak?std::array<float,3>{.55f,.08f,.75f}:std::array<float,3>{1,.7f,.18f};
    return {1-depth+depth*hue[0],1-depth+depth*hue[1],1-depth+depth*hue[2]};
}
inline constexpr std::array<const char*,6> starPowerColorNames{{"DEFAULT","PURPLE","WHITE","GOLD","RED","GREEN"}};
inline constexpr std::array<std::array<float,3>,6> starPowerColors{{
    {{0,1,1}},{{.68f,.32f,1}},{{1,1,1}},{{1,.72f,.12f}},{{1,.14f,.12f}},{{.16f,1,.25f}}
}};
}
