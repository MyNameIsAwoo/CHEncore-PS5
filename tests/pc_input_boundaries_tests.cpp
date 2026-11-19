// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/guitar_session.hpp"
#include <cassert>
#include <cstdio>

int main() {
    using namespace rhythm;
    const HitWindow window{};
    for(double head:{.5,.625,.75,1.,1.125}) {
        const auto end=window.lateDeadline(head);
        assert(!window.contains(head,end));
        assert(window.contains(head,std::nextafter(end,-INFINITY)));
        SongChart chart{{{head,0,1}}};InputEvaluator evaluator(chart);
        const auto provider=[&](size_t,const Note&){return window;};
        assert(evaluator.expireNotes(std::nextafter(end,-INFINITY),provider).empty());
        assert(evaluator.expireNotes(end,provider)==std::vector<size_t>{0});
    }
    // Independently measured PC miss at .6950000000000001: the buffered
    // strum is protected by the live HOPO window before the miss breaks streak.
    SongChart chart{{{.5,0,1},{.625,0,2,false,true},{.75,0,4,false,true}}};
    GuitarSession session(chart);
    session.update({},0);session.update({1,false,true},.5);
    session.update({1,false,false},.5083333333333333);
    session.update({0,false,true},.6333333333333333);
    session.update({},.7);
    assert(session.events().size()==1);
    assert(session.events()[0].kind==GuitarEventKind::Miss);
    assert(session.events()[0].time==.6950000000000001);
    // A second physical strum consumes an existing buffer before a new hit.
    SongChart repeated{{{.5,0,1},{1.125,0,4}}};GuitarSession retry(repeated);
    retry.update({},0);retry.update({1,false,true},.5);
    retry.update({},.6);retry.update({2,false,true},1.1);
    retry.update({4,true,false},1.125);
    assert(retry.events().size()==2);
    assert(retry.events()[0].kind==GuitarEventKind::Overstrum);
    assert(retry.events()[1].kind==GuitarEventKind::Hit);
    assert(retry.hits()==2 && retry.overstrums()==1);
    SongChart blocked{{{.5,0,1},{.625,0,2,false,true},{.75,0,16,false,true}}};
    GuitarSession buffered(blocked);
    buffered.update({},0);buffered.update({1,false,true},.5);buffered.update({},.6);
    buffered.update({31,false,true},.625);
    assert(!buffered.update({31,false,false},.6833333333333333));
    assert(buffered.hits()==1 && buffered.misses()==0);
    buffered.update({16,true,false},.75);
    assert(buffered.hits()==2 && buffered.misses()==1);
    SongChart chord{{{.5,0,1},{.625,0,6},{.75,0,8,false,true}}};
    GuitarSession stale(chord);
    stale.update({},0);stale.update({1,false,true},.5);stale.update({},.6);
    stale.update({31,false,true},.625);stale.update({31,false,false},.6833333333333333);
    stale.update({31,false,false},.7);
    assert(stale.events().size()==2);
    assert(stale.events()[0].kind==GuitarEventKind::Overstrum && stale.events()[1].kind==GuitarEventKind::Miss);
    SongChart recovery{{{.5,0,1},{.75,0,8,false,true},{1,0,8,false,true}}};
    GuitarSession recovered(recovery);
    recovered.update({},0);recovered.update({},.6);
    recovered.update({8,false,true},.75);recovered.update({8,false,false},.8);
    recovered.update({8,false,false},.95);
    assert(recovered.hits()==2 && recovered.misses()==1);
    SongChart heldTap{{{2,0,8,true}}};
    GuitarSession normalHeld(heldTap),precisionHeld(heldTap);
    precisionHeld.setPrecisionMode(true);
    normalHeld.update({8,false,false},0);precisionHeld.update({8,false,false},0);
    normalHeld.update({8,false,false},1.1);precisionHeld.update({8,false,false},1.1);
    normalHeld.update({8,false,false},1.94);precisionHeld.update({8,false,false},1.94);
    assert(normalHeld.hits()==1 && precisionHeld.hits()==0);
    SongChart overlapping{{{.5,0,1,false,false,false,false,true},{.54,0,8,false,true}}};
    GuitarSession previousHeld(overlapping);previousHeld.setSustainMasks(8);
    previousHeld.update({},0);previousHeld.update({9,false,true},.49);
    assert(previousHeld.hits()==1);
    previousHeld.update({1,false,false},.51);
    assert(previousHeld.hits()==2);
    std::puts("PASS timing boundaries, persistent normal fret state and previous-input candidate refresh");
}
