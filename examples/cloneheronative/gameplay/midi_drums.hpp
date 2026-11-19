// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rhythm::midi {
struct UsbInputEndpoint {
    uint8_t interface{},address{};
    uint16_t packetBytes{};
};
// USB-IF MIDI 1.0, sections 6.1/6.2: Audio/MIDIStreaming interface,
// bcdMSC=1.00 and bulk IN. Do not claim AudioStreaming, HID, storage or MIDI 2.0.
inline unsigned inputEndpoints(std::span<const uint8_t> descriptors,
                               std::span<UsbInputEndpoint> output) noexcept {
    unsigned count=0;uint8_t interface=0;
    bool eligible=false,midi1=false;
    for(size_t offset=0;offset<descriptors.size();){
        if(descriptors.size()-offset<2)return 0;
        const auto* d=descriptors.data()+offset;const unsigned length=d[0];
        if(length<2 || length>descriptors.size()-offset)return 0;
        if(d[1]==4){
            if(length<9)return 0;
            interface=d[2];midi1=false;
            eligible=d[3]==0 && d[5]==1 && d[6]==3 && d[7]==0;
        }else if(eligible && d[1]==0x24 && length>=7 && d[2]==1){
            midi1=d[3]==0 && d[4]==1;
        }else if(eligible && midi1 && d[1]==5){
            if(length<7)return 0;
            const unsigned packet=d[4]|(unsigned(d[5])<<8);
            if((d[2]&0x80) && (d[2]&15) && (d[3]&3)==2 && packet>=4 && packet<=512 && count<output.size())
                output[count++]={interface,d[2],uint16_t(packet)};
        }
        offset+=length;
    }
    return count;
}
inline constexpr const char* navigationNotice =
    "A DualSense controller is required for menu navigation when using MIDI drums. "
    "MIDI inputs can cause unintended menu selections.";

struct Strike {
    uint64_t receivedUs{};
    uint8_t cable{},channel{},note{},velocity{};
    bool operator==(const Strike&) const noexcept = default;
};

// One decoder per physical USB MIDI IN endpoint. USB MIDI 1.0 event packets
// contain a cable/CIN byte followed by up to three MIDI bytes. MIDIInputManager
// 0x18022B150 accepts positive-velocity Note On only; Note Off, clock, SysEx and
// velocity-zero Note On never become strikes. A repeated Note On IS a new hit.
class UsbDecoder {
public:
    void reset() noexcept { used_=0; }
    template<class Emit>
    void feed(std::span<const uint8_t> data,uint64_t receivedUs,Emit&& emit) noexcept {
        for(const auto value:data) {
            packet_[used_++]=value;
            if(used_!=4)continue;
            used_=0;
            if((packet_[0]&15)!=9 || (packet_[1]&240)!=144 ||
               packet_[2]>127 || !packet_[3] || packet_[3]>127)continue;
            emit(Strike{receivedUs,uint8_t(packet_[0]>>4),uint8_t(packet_[1]&15),packet_[2],packet_[3]});
        }
    }
private:
    std::array<uint8_t,4> packet_{};
    unsigned used_{};
};

// The backend synchronizes producer/consumer access. Different controllers own
// different queues; identical strikes are never coalesced like held pad states.
template<size_t Capacity=512>
class StrikeQueue {
    static_assert(Capacity>0);
public:
    void reset() noexcept { head_=count_=dropped_=0; }
    void push(const Strike& strike) noexcept {
        if(count_==Capacity){head_=(head_+1)%Capacity;--count_;++dropped_;}
        data_[(head_+count_)%Capacity]=strike;++count_;
    }
    template<class Emit> unsigned drain(Emit&& emit) noexcept {
        for(size_t i=0;i<count_;++i)emit(data_[(head_+i)%Capacity]);
        const unsigned dropped=dropped_;reset();return dropped;
    }
    size_t size() const noexcept { return count_; }
    void accountDropped(unsigned count) noexcept { dropped_+=count; }
private:
    std::array<Strike,Capacity> data_{};
    size_t head_{},count_{};
    unsigned dropped_{};
};

enum class Layout : uint8_t { Pro4,FiveLane };
enum class Action : uint8_t { Kick,Red,YellowPad,YellowCymbal,BluePad,BlueCymbal,GreenPad,GreenCymbal,OrangeCymbal };
inline constexpr uint16_t actionMask(Action action) noexcept {
    switch(action){
    case Action::Kick:return 1;case Action::Red:return 2;
    case Action::YellowPad:return 4;case Action::BluePad:return 8;
    case Action::GreenPad:return 16;case Action::YellowCymbal:return 32;
    case Action::BlueCymbal:return 64;case Action::GreenCymbal:case Action::OrangeCymbal:return 128;
    }return 0;
}
inline constexpr bool actionAllowed(Layout layout,Action action) noexcept {
    if(layout==Layout::Pro4)return action!=Action::OrangeCymbal;
    return action==Action::Kick || action==Action::Red || action==Action::YellowCymbal ||
        action==Action::BluePad || action==Action::OrangeCymbal || action==Action::GreenPad;
}
struct VelocityRange {
    uint8_t soft{},hard{};
    bool calibrated() const noexcept { return soft>0 && soft<hard && hard<=127; }
    bool capture(unsigned low,unsigned high) noexcept {
        if(!low || low>=high || high>127)return false;
        soft=uint8_t(low);hard=uint8_t(high);return true;
    }
    int normalize(unsigned velocity) const noexcept {
        velocity=std::min(velocity,127u);
        if(!calibrated())return int(velocity);
        return std::clamp((int(velocity)-int(soft))*127/(int(hard)-int(soft)),0,127);
    }
};
struct Binding {
    Action action{};
    uint8_t cable{},channel{},note{},minimumVelocity{10};
    VelocityRange velocity;
    bool matches(const Strike& strike) const noexcept {
        return strike.velocity>0 && strike.velocity<=127 && cable==strike.cable &&
            channel==strike.channel && note==strike.note;
    }
};
struct MappedStrike {
    uint64_t receivedUs{};
    uint16_t mask{};
    int velocity{-1};
};
// Portable mapping layer. It supplies drum events only; menu navigation and the
// player's Start/Select remain on the companion DualSense. Kicks intentionally
// have no velocity threshold/calibration or shared retrigger timer.
class Mapping {
public:
    static constexpr unsigned capacity=16;
    explicit Mapping(Layout layout=Layout::Pro4):layout_(layout){}
    Layout layout() const noexcept { return layout_; }
    std::span<const Binding> bindings() const noexcept { return {bindings_.data(),count_}; }
    bool bind(Binding binding) noexcept {
        if(!actionMask(binding.action) || !actionAllowed(layout_,binding.action) || binding.cable>15 || binding.channel>15 ||
           binding.note>127 || binding.minimumVelocity>127)return false;
        if(binding.action==Action::Kick){binding.minimumVelocity=1;binding.velocity={};}
        else if((binding.velocity.soft || binding.velocity.hard) && !binding.velocity.calibrated())return false;
        else if(binding.velocity.calibrated())binding.minimumVelocity=std::min(binding.minimumVelocity,binding.velocity.soft);
        // One physical note must not score two different actions. A second kick
        // may share a MIDI note: keep one binding and preserve every event.
        for(unsigned i=0;i<count_;++i)if(bindings_[i].cable==binding.cable &&
            bindings_[i].channel==binding.channel && bindings_[i].note==binding.note){
            if(bindings_[i].action!=binding.action)return false;
            bindings_[i]=binding;return true;
        }
        if(count_==capacity)return false;
        bindings_[count_++]=binding;return true;
    }
    template<class Emit> void apply(const Strike& strike,Emit&& emit) const noexcept {
        for(const auto& binding:bindings())if(binding.matches(strike)){
            if(binding.action==Action::Kick)emit(MappedStrike{strike.receivedUs,1,-1});
            else if(strike.velocity>=binding.minimumVelocity)
                emit(MappedStrike{strike.receivedUs,actionMask(binding.action),binding.velocity.normalize(strike.velocity)});
            return;
        }
    }
private:
    Layout layout_;
    std::array<Binding,capacity> bindings_{};
    unsigned count_{};
};

// Mapping order is separate from calibration: the latter must never ask for a
// soft/hard kick. A double pedal on a single pad uses the one-pad setup.
struct SetupOrder {
    std::array<Action,9> actions{};
    unsigned count{};
};
inline SetupOrder setupOrder(Layout layout,bool twoKickPads,bool calibration=false) noexcept {
    SetupOrder order;
    const auto add=[&](Action a){order.actions[order.count++]=a;};
    if(!calibration){add(Action::Kick);if(twoKickPads)add(Action::Kick);}
    add(Action::Red);
    if(layout==Layout::Pro4){
        add(Action::YellowPad);add(Action::YellowCymbal);add(Action::BluePad);
        add(Action::BlueCymbal);add(Action::GreenPad);add(Action::GreenCymbal);
    }else{
        add(Action::YellowCymbal);add(Action::BluePad);add(Action::OrangeCymbal);add(Action::GreenPad);
    }
    return order;
}
}
