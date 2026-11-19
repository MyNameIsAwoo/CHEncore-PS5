// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include "midi_profile.hpp"
#include <limits>
namespace rhythm::midi {
// UI-independent state for the DualSense-guided MIDI popup. Controller buttons
// choose pages; MIDI events only provide mappings or calibration samples.
class SetupWizard {
public:
    enum class Page {Layout,KickPads,Capture,OfferVelocity,Soft,Hard,Review};
    Page page{Page::Layout};
    Profile draft;
    const char* message{navigationNotice};
    void begin(uint16_t vid,uint16_t pid,const std::string& name){
        draft={};draft.vid=vid;draft.pid=pid;draft.name=name;
        page=Page::Layout;index_=0;readyAfterUs_=0;message=navigationNotice;
    }
    bool selectLayout(Layout layout){
        if(page!=Page::Layout || (layout!=Layout::Pro4 && layout!=Layout::FiveLane))return false;
        draft.mapping=Mapping(layout);page=Page::KickPads;return true;
    }
    bool selectKickPads(bool two,uint64_t nowUs){
        if(page!=Page::KickPads)return false;
        draft.twoKickPads=two;order_=setupOrder(draft.mapping.layout(),two);index_=0;
        page=Page::Capture;readyAfterUs_=nowUs;message="Strike the requested pad once.";return true;
    }
    Action action() const noexcept {return index_<order_.count?order_.actions[index_]:Action::Kick;}
    bool secondKick() const noexcept {return page==Page::Capture && draft.twoKickPads && index_==1;}
    bool chooseVelocity(bool calibrate,uint64_t nowUs){
        if(page!=Page::OfferVelocity)return false;
        if(!calibrate){page=Page::Review;message="Review and save the MIDI mapping.";return true;}
        order_=setupOrder(draft.mapping.layout(),draft.twoKickPads,true);index_=0;
        page=Page::Soft;readyAfterUs_=nowUs;message="Strike this pad softly.";return true;
    }
    // Recalibrate an existing player's kit without changing note assignments,
    // kick count or another player's/device-default profile.
    void beginVelocity(const Profile& profile,uint64_t nowUs){
        draft=profile;page=Page::OfferVelocity;chooseVelocity(true,nowUs);
        const auto original=order_;order_={};
        for(unsigned i=0;i<original.count;++i) {
            const auto action=original.actions[i];
            if(std::any_of(draft.mapping.bindings().begin(),draft.mapping.bindings().end(),
                [&](const auto& binding){return binding.action==action;}))order_.actions[order_.count++]=action;
        }
        if(!order_.count){page=Page::Review;message="No mapped pads or cymbals to calibrate.";}
    }
    bool skipVelocity(uint64_t nowUs){
        if(page!=Page::Soft && page!=Page::Hard)return false;
        advanceVelocity(nowUs);return true;
    }
    bool sample(const Strike& strike){
        if(!strike.velocity || strike.velocity>127 || strike.receivedUs<readyAfterUs_)return false;
        if(page==Page::Capture){
            Binding binding;binding.action=action();binding.cable=strike.cable;
            binding.channel=strike.channel;binding.note=strike.note;
            if(!draft.mapping.bind(binding)){
                message="That input is already assigned to another pad. Strike a different pad.";return false;
            }
            ++index_;waitForNext(strike.receivedUs);
            if(index_==order_.count){page=Page::OfferVelocity;message="Calibrate pad and cymbal velocity? Kicks do not need calibration.";}
            return true;
        }
        if(page!=Page::Soft && page!=Page::Hard)return false;
        const auto list=draft.mapping.bindings();
        const auto found=std::find_if(list.begin(),list.end(),[&](const auto& binding){return binding.action==action() && binding.matches(strike);});
        if(found==list.end()){message="Strike the pad shown in the prompt.";return false;}
        if(page==Page::Soft){soft_=strike.velocity;page=Page::Hard;message="Strike the same pad firmly.";waitForNext(strike.receivedUs);return true;}
        auto binding=*found;
        if(!binding.velocity.capture(soft_,strike.velocity)){
            message="The hard hit must be stronger. Try again, or skip this pad.";waitForNext(strike.receivedUs);return false;
        }
        if(!draft.mapping.bind(binding))return false;
        advanceVelocity(strike.receivedUs);return true;
    }
private:
    void waitForNext(uint64_t nowUs) noexcept {
        // Setup-page transitions only. Gameplay has no shared kick debounce.
        constexpr uint64_t delay=250000;
        readyAfterUs_=nowUs>std::numeric_limits<uint64_t>::max()-delay?std::numeric_limits<uint64_t>::max():nowUs+delay;
    }
    void advanceVelocity(uint64_t nowUs){
        ++index_;soft_=0;waitForNext(nowUs);
        if(index_==order_.count){page=Page::Review;message="Review and save the MIDI mapping.";}
        else {page=Page::Soft;message="Strike this pad softly.";}
    }
    SetupOrder order_{};unsigned index_{};uint8_t soft_{};uint64_t readyAfterUs_{};
};
}
