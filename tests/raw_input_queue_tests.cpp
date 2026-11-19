// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/raw_input_queue.hpp"
#include "gameplay/controller_profile.hpp"
#include "gameplay/guitar_session.hpp"
#include <cassert>
#include <cstdio>
#include <cmath>
int main() {
    using namespace rhythm;using namespace rhythm::controls;
    RawInputQueue queue;RawInputBatch batch;
    RawInputReport report;report.length=27;report.bytes[0]=2;report.bytes[2]=4;report.receivedUs=990000;
    queue.push(report);report.bytes[2]=8;report.receivedUs=1000000;queue.push(report);
    queue.drain(batch);assert(batch.count==2 && !batch.dropped);
    ControllerProfile profile;std::string error;
    assert(loadProfile("assets/controller_profiles/12ba_0100.json",profile,error));
    SongChart chart{{{1.,0.,1}}};GuitarSession game(chart),latestOnly(chart);
    (void)game.update({},.98);(void)latestOnly.update({},.98);
    double previous=.98;
    for(unsigned i=0;i<batch.count;++i) {
        const auto& raw=batch.reports[i];const auto logical=applyProfile(profile,{},raw.bytes.data(),raw.length);
        const auto at=rawInputSongTime(raw.receivedUs,1020000,1.02,previous,1.);
        (void)game.update({logical.frets,logical.strumUp,logical.strumDown},at);previous=at;
    }
    (void)game.update({1,false,false},1.02);assert(game.hits()==1);
    (void)latestOnly.update({1,false,false},1.02);assert(latestOnly.hits()==0);
    queue.drain(batch);assert(!batch.count && !batch.dropped);
    for(unsigned i=0;i<RawInputBatch::capacity+6;++i){report.receivedUs=i;queue.push(report);}
    queue.drain(batch);assert(batch.count==RawInputBatch::capacity && batch.dropped==6);
    for(unsigned i=0;i<batch.count;++i)assert(batch.reports[i].receivedUs==i+6);
    queue.drain(batch);assert(!batch.count && !batch.dropped);
    queue.push(report);queue.reset();queue.drain(batch);assert(!batch.count);
    assert(std::abs(rawInputSongTime(990000,1020000,1.02,.98,1.)-.99)<1e-12);
    assert(rawInputSongTime(500000,1020000,1.02,1.,1.)==1.);
    assert(rawInputSongTime(2000000,1020000,1.02,1.,1.)==1.02);
    assert(std::abs(rawInputSongTime(1000000,1020000,1.02,.98,.5)-1.01)<1e-12);
    assert(rawInputSongTime(1000000,1020000,.5,1.,1.)==.5);
    puts("PASS between-frame guitar pulse, report order, overflow/reset and audio-time mapping");
}
