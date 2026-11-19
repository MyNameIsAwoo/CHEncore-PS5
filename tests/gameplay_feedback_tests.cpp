// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#include "../examples/cloneheronative/gameplay/gameplay_audio.hpp"
#include "../examples/cloneheronative/gameplay/guitar_particles.hpp"
#include "../examples/cloneheronative/gameplay/audio_player.hpp"
#include <cassert>
#include <cstdio>
#include <array>
#include <cmath>
int main(int argc,char** argv) {
    assert(argc==2);
    for(int instrument=0;instrument<4;++instrument)for(size_t stem=0;stem<14;++stem) {
        assert(rhythm::missStemGain(instrument,stem,false,0)==1);
        assert(rhythm::missStemGain(instrument,stem,true,.25f)==(rhythm::controlledStem(instrument,stem)?.25f:1));
        if(stem==12 || stem==13)assert(rhythm::missStemGain(instrument,stem,true,0)==1);
    }
    struct Spec {float origin[3]={0,0,0};float basis[3][3]={{1,0,0},{0,1,0},{0,0,1}};float life=.3f,speed=.7f,size=.05f,gravity=.4f,simulation=1,angle=20,radius=.01f;} spec;
    rhythm::GuitarParticles pool;
    for(unsigned i=0;i<1000;++i)pool.emit(spec,i%5,false,false);
    assert(pool.size()==pool.Capacity);
    pool.visit([&](const auto& p){assert(p.lane<5 && p.life==spec.life);assert(p.velocity[2]>0);assert(std::abs(std::sqrt(p.velocity[0]*p.velocity[0]+p.velocity[1]*p.velocity[1]+p.velocity[2]*p.velocity[2])-spec.speed)<1e-5);});
    pool.update(0);assert(pool.size()==pool.Capacity);
    pool.update(.31f);assert(pool.size()==0);pool.reset();
    rhythm::AudioStemMixer mixer;assert(mixer.loadUiSounds(argv[1]));
    std::array<int16_t,512> output{};
    for(size_t cue=0;cue<static_cast<size_t>(rhythm::AudioStemMixer::UiSound::Count);++cue) {
        const auto previous=mixer.effectBlocks();mixer.playUiSound(static_cast<rhythm::AudioStemMixer::UiSound>(cue));
        for(unsigned i=0;i<1000;++i)rhythm::AudioStemMixer::audioFill(output.data(),256,&mixer);
        assert(mixer.effectBlocks()>previous);
    }
    std::puts("PASS instrument-only miss gates, fixed particle bounds/expiry, and all 17 original sound effects mixed to PCM");
}
