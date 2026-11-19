// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/audio_player.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
using Mixer=rhythm::AudioStemMixer;
uint64_t energy(Mixer::UiSound sound,float gain) {
    Mixer audio;assert(audio.loadUiSounds("assets/pc_ui_sounds"));
    audio.setUiGain(.5f);audio.setMissSoundGain(gain);audio.playUiSound(sound);
    std::array<int16_t,512> block;uint64_t sum=0;
    for(int i=0;i<200;++i){Mixer::audioFill(block.data(),256,&audio);for(auto v:block)sum+=std::abs(int(v));}
    return sum;
}
int main() {
    const auto full=energy(Mixer::UiSound::BadNote1,1),half=energy(Mixer::UiSound::BadNote1,.5);
    assert(full>0 && energy(Mixer::UiSound::BadNote1,0)==0 && half>full*.49 && half<full*.51);
    assert(energy(Mixer::UiSound::Confirm,1)>0);
    assert(energy(Mixer::UiSound::Confirm,1)==energy(Mixer::UiSound::Confirm,0));
    std::puts("PASS independent miss volume: mute/half/full PCM; confirm remains audible and unchanged");
}
