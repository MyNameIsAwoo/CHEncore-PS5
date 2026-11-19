// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/pc_resume.hpp"
#include "gameplay/pc_sustain.hpp"
#include <cassert>
#include <cstdio>
int main() {
    using namespace rhythm;
    for(double fps:{30,60,120,240}) {
        PcResume resume;PcSustainGrace held;
        held.sample(10,true);assert(resume.begin(10,10,false));
        // Neither pause time nor repeated lead-in settles/releases held tails.
        unsigned completions=0;double clock=10;
        for(unsigned i=0;i<unsigned(fps*3);++i) {
            if(resume.animated) {if(resume.advance(1/fps))++completions;clock=resume.position;}
            else clock+=1/fps;
            if(!resume.protectedAt(clock))held.sample(clock,true);
        }
        assert(completions==1 && !held.dropped() && held.scoring() && !resume.catchingUp);
        assert(std::abs(resume.position-8.5)<1e-12);
        resume.reset();assert(!resume.animated && !resume.catchingUp);
    }
    PcResume resume;
    assert(!resume.begin(1,1,false));assert(!resume.begin(10,10,true));
    assert(resume.begin(10,10,false));assert(!resume.advance(0));
    resume.advance(.25);assert(std::abs(resume.position-9.5)<1e-12);
    assert(!resume.begin(9.5,10,false)); // pause during rewind must not restart it
    assert(resume.advance(.5));assert(resume.protectedAt(8.5));
    assert(!resume.begin(9,9,false));assert(resume.protectedAt(9.999));assert(!resume.protectedAt(10));
    assert(resume.begin(12,12,false));assert(resume.advance(2));assert(resume.position==10.5);
    std::puts("PASS PC resume rollback, duration, catch-up protection, repeated pauses and held sustain at all cadences");
}
