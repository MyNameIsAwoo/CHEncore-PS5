// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#include "gameplay/drum_session.hpp"
#include <cassert>
#include <cstdio>
int main(){
    using namespace rhythm;
    pc_drums::PadEdges edges;
    assert(edges.sample(pc_drums::fallbackPads(18))==3); // kick+red
    assert(edges.sample(pc_drums::fallbackPads(18))==0); // held is not a second strike
    assert(edges.sample(pc_drums::fallbackPads(22))==4); // independent added yellow
    edges.prime(pc_drums::fallbackPads(22)); // connection/resume suppresses held pads
    assert(edges.sample(pc_drums::fallbackPads(22))==0);
    assert(edges.sample(0)==0);assert(edges.sample(2)==2);
    SongChart chart;chart.kind=ChartKind::Drums;
    chart.notes={{0.5,0,2},{0.5,0,4},{0.75,0,8},{1.0,0,16}};
    DrumSession session(chart);session.beginSection(0.7);
    session.update({},0.71);session.prime();
    DrumInput held;held.strikes=8;session.update(held,0.75);
    assert(session.hits()==1 && session.misses()==0);
    assert(session.evaluator().state(2)==NoteState::Hit);
    session.update({},1.2);assert(session.misses()==1);
    session.beginSection(0.7);
    unsigned callbacks=0;session.update({},1.2,{},[&](const DrumEvent& e){if(e.kind==DrumEventKind::Hit)++callbacks;},true);
    assert(callbacks==2 && session.hits()==2 && session.misses()==0);
    // Bot emits every independent sibling at its authored time, even when a
    // slow render frame spans multiple heads. Reset restores activation state.
    session.reset();callbacks=0;
    session.update({},1.2,{},[&](const DrumEvent& e){if(e.kind==DrumEventKind::Hit){++callbacks;assert(e.time==chart.notes[e.noteIndex].time);}},true);
    assert(callbacks==4 && session.hits()==4 && session.misses()==0);
    std::puts("PASS drum input edges, resume/section reset and bot timing");
}
