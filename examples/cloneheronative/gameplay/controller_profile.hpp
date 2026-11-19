// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "controller_mapping.hpp"
namespace rhythm::controls {
// Portable contract: neither input samples nor stored profiles contain scePad,
// USB, operating-system handles, or physical player-slot numbers.
enum class Source {Native,RawHid};
enum class Kind {Digital,Hat,Analog};
struct Locator {
    Kind kind{Kind::Digital};int byte{},bytes{1},value{};uint32_t mask{};
    std::string padButton,padAxis;float rest{},min{},max{255};bool invert{};
};
struct Binding {std::string action;Locator input;bool hasVelocity{};Locator velocity;std::vector<Locator> alternatives;};
struct ControllerProfile {
    unsigned schema{1};uint16_t vid{},pid{};std::string name,instrument{"guitar_5fret"};
    Source source{Source::Native};unsigned reportLength{};int reportId{-1};
    int menuX{-1},menuY{-1},menuHat{-1};uint32_t menuHatMask{255};
    // Schema 3: fixed hardware menu controls remain separate from gameplay remaps.
    std::vector<Binding> menuBindings;
    bool suppressHat{};Locator hatSuppression;
    std::vector<Binding> bindings;
};
struct NativeSample {uint32_t buttons{};std::array<float,6> axes{};};
struct InstrumentState {
    uint16_t frets{},pads{};std::array<float,9> velocities{};uint16_t velocityKnown{};
    bool strumUp{},strumDown{},start{},starPower{};float whammy{},tilt{};
    bool operator==(const InstrumentState&) const noexcept = default;
};
bool parseProfile(const std::string& json,ControllerProfile& out,std::string& error);
std::string writeProfile(const ControllerProfile& profile);
bool loadProfile(const char* path,ControllerProfile& out,std::string& error);
bool saveProfile(const char* path,const ControllerProfile& profile,std::string& error);
InstrumentState applyProfile(const ControllerProfile&,const NativeSample&,const uint8_t* report=nullptr,unsigned size=0) noexcept;
bool sameControllerInput(const ControllerProfile&,const uint8_t* a,unsigned aSize,const uint8_t* b,unsigned bSize) noexcept;
ControllerProfile nativeProfile(int type,const Mapping&,uint16_t vid=0,uint16_t pid=0);
bool nativeMapping(const ControllerProfile&,Mapping&,int& type) noexcept;
uint32_t nativeButton(const std::string&) noexcept;
const char* instrumentName(int type) noexcept;
}
namespace rhythm::controls {
struct ReportCapture {
    std::array<uint8_t,64> baseline{},previous{};
    unsigned length{};
    void begin(const uint8_t* report,unsigned size) noexcept;
    bool capture(const uint8_t* report,unsigned size,bool hat,Locator& out,std::string& error,
                 const ControllerProfile* known=nullptr);
};
bool sameLocator(const Locator&,const Locator&) noexcept;
// Per-wizard USB input history. Distinct pads/frets remain immediately eligible.
struct CapturePressFilter {
    std::array<Locator,16> inputs{};
    std::array<double,16> acceptedAt{};
    unsigned count{},next{};
    bool accept(const Locator& input,double now,double interval=.075) noexcept;
};
}
