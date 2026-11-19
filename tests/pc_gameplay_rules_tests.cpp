#include "gameplay/guitar_session.hpp"
#include "gameplay/pc_star_power.hpp"
#include "gameplay/pc_chart_rules.hpp"
#include <cstdio>
#include <cstdlib>

void check(bool value,const char* label) {
    if(!value) {std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}
}
int main() {
    using rhythm::GuitarHead;
    check(rhythm::naturalGuitarHopo(1,2,65,65),"Open can be a natural HOPO at inclusive threshold");
    check(rhythm::naturalGuitarHopo(2,1,64,65),"Open-to-fret natural HOPO");
    check(!rhythm::naturalGuitarHopo(2,2,1,65),"repeated single remains a strum");
    check(!rhythm::naturalGuitarHopo(6,2,1,65),"chord is not naturally legato");
    check(rhythm::naturalGuitarHopo(2,6,1,65),"chord-to-single allowed by classifier");
    check(!rhythm::naturalGuitarHopo(2,6,1,65,true),"optional chord subset restriction");
    check(rhythm::naturalGuitarHopo(8,6,1,65,true),"unrelated chord-to-single remains allowed");
    check(!rhythm::naturalGuitarHopo(2,0,1,65) &&
        !rhythm::naturalGuitarHopo(2,4,66,65),"first note and excessive gap strum");
    check(rhythm::chartGuitarHead(true,true,false)==GuitarHead::Strum,"N5 flips natural HOPO to strum");
    check(rhythm::chartGuitarHead(false,true,false)==GuitarHead::Hopo,"N5 flips strum to HOPO");
    check(rhythm::chartGuitarHead(true,true,true)==GuitarHead::Tap,"N6 tap overrides flip");
    check(rhythm::modifiedGuitarHead(GuitarHead::Tap,2)==GuitarHead::Hopo,"All HOPOs clears tap flag");
    check(rhythm::modifiedGuitarHead(GuitarHead::Hopo,1)==GuitarHead::Strum,"All Strums changes head type");
    check(rhythm::modifiedGuitarHead(GuitarHead::Hopo,15)==GuitarHead::Strum,"All Opens has highest precedence");
    check(rhythm::modifiedGuitarHead(GuitarHead::Strum,7)==GuitarHead::Tap && !rhythm::allStrumsRequired(7),"All Taps beats HOPOs/Strums");
    check(rhythm::modifiedGuitarHead(GuitarHead::Hopo,16)==GuitarHead::Tap,"HOPOs to Taps");
    // Strummed red with green anchored, then pull off red to a green HOPO.
    rhythm::SongChart pullOff{{{1,0,2},{1.2,0,1,false,true}}};
    rhythm::GuitarSession guitar(pullOff);
    check(guitar.update({3,false,true},1).has_value(),"anchored single strum");
    guitar.update({3,false,false},1.1);
    check(guitar.update({1,false,false},1.2).has_value(),"pull-off is judged on release");
    check(guitar.hits()==2 && !guitar.misses(),"pull-off keeps the chain");

    rhythm::SongChart recovery{{{1,0,1},{1.5,0,2,false,true},{2,0,4,true}}};
    rhythm::GuitarSession broken(recovery);
    broken.update({},1.1);
    check(broken.misses()==1,"expired head breaks chain before input");
    check(!broken.update({2,false,false},1.5),"HOPO cannot recover missed chain alone");
    check(broken.update({2,false,true},1.51).has_value(),"strum recovers HOPO");
    check(broken.update({4,false,false},2).has_value(),"tap is independent of previous strum");

    rhythm::SongChart firstHopo{{{1,0,2,false,true}}};
    rhythm::GuitarSession start(firstHopo);
    check(start.update({2,false,false},1).has_value(),"PC first-HOPO exception before any break");
    start.reset();start.update({1,false,true},.5);
    check(start.update({2,false,false},1).has_value(),"pre-song strum preserves first-HOPO exception");

    rhythm::SongChart taps{{{1,0,4,true},{2,0,8,true}}};
    rhythm::GuitarSession early(taps);
    check(!early.update({4,false,false},.8),"early fret does not hit outside window");
    check(early.update({4,false,false},.95).has_value(),"held tap hits when window opens");
    early.update({},1.2);
    early.update({8,false,false},1.3);
    check(early.update({8,false,false},1.95).has_value(),"one-second PC fret timer");
    early.reset();early.update({4,false,false},-.1);
    check(!early.update({4,false,false},1),"stale fret timer cannot hit tap");

    rhythm::SongChart single{{{1,0,2}}};
    rhythm::GuitarSession buffered(single);
    check(!buffered.update({1,false,true},.96),"wrong fret initially waits");
    check(buffered.overstrums()==0,"no premature overstrum");
    check(buffered.update({2,false,true},1).has_value(),"frets can settle after strum");
    check(buffered.overstrums()==0,"corrected buffer preserves streak");
    buffered.reset();buffered.update({1,false,true},.95);buffered.update({1,false,false},1.04);
    check(buffered.overstrums()==0,"pre-play buffer expiry does not break streak");
    buffered.reset();buffered.update({2,false,true},.80);
    check(buffered.overstrums()==0,"distant pre-play strum is ignored");

    rhythm::SongChart skipped{{{1,0,1},{1.03,0,2}}};
    rhythm::GuitarSession skip(skipped);
    const auto hit=skip.update({2,false,true},1.03);
    check(hit && hit->noteIndex==1 && skip.misses()==1,"later strum retires skipped head");
    check(skip.evaluator().state(0)==rhythm::NoteState::Missed,"skipped head cannot be hit afterwards");
    rhythm::SongChart noSkip{{{.5,0,1},{1,0,2},{1.03,0,4,false,true}}};
    rhythm::GuitarSession chained(noSkip);
    chained.update({1,false,true},.5);chained.update({},.6);
    check(!chained.update({4,false,false},1.03),"HOPO cannot jump an unplayed earlier strum");

    guitar.reset();guitar.update({3,false,true},1);guitar.update({3,false,false},1.1);
    guitar.update({1,false,false},1.2);guitar.update({1,false,true},1.21);
    check(guitar.overstrums()==0,"96ms HOPO restrum protection");
    guitar.update({},1.22);guitar.update({1,true,false},1.4);
    check(guitar.overstrums()==0,"post-chart strum without tails is ignored");

    start.reset();start.setAllStrums(true);
    check(!start.update({2,false,false},1),"All Strums suppresses legato");
    check(start.update({2,false,true},1.01).has_value(),"All Strums accepts edge");
    early.reset();early.prime({4,false,true});
    check(!early.update({4,false,true},1),"resume does not invent tap or strum");

    rhythm::GuitarSession ghost(pullOff);ghost.setNoFretGhosting(true);
    check(ghost.update({3,false,true},1).has_value(),"No Fret Ghosting retains lower anchoring");
    ghost.update({3,false,false},1.02);
    ghost.update({19,false,false},1.05);ghost.update({3,false,false},1.06);
    ghost.update({3,false,false},1.08);
    check(ghost.update({1,false,false},1.2).has_value(),"ghost release still allows matching HOPO");
    check(ghost.streakBreaks()==1 && ghost.overstrums()==0,"unused fret ghost breaks streak without overstrum");
    ghost.reset();ghost.update({3,false,true},1);ghost.update({3,false,false},1.1);
    ghost.update({1,false,false},1.2);ghost.update({1,false,false},1.22);
    check(ghost.streakBreaks()==0,"used red fret pull-off is not a ghost");

    // Measured PC windows and subframe hits (see mechanics audit).
    rhythm::SongChart dense{{{.5,0,1},{1,0,2,true},{400.0/384,0,4,true},{420.0/384,0,8,true}}};
    rhythm::GuitarSession precision(dense);precision.setPrecisionMode(true);
    check(precision.noteWindow(0).early==.04999995 &&
        precision.noteWindow(1).early==.04125393333333333 &&
        precision.noteWindow(2).early==.03394640625,
        "Precision windows use chronological neighbor spacing and PC arithmetic");
    rhythm::SongChart stalled{{{.5,0,1},{1,0,2,false,true}}};
    rhythm::GuitarSession gap(stalled);
    gap.update({},0);gap.update({1,false,true},.5);
    gap.update({1,false,false},.516);gap.update({2,false,false},.85);
    gap.update({2,false,false},1.2);
    check(gap.events().size()==1 && gap.events()[0].kind==rhythm::GuitarEventKind::Hit &&
        gap.events()[0].time==.93,"held HOPO hits at window entry during stalled frame");
    rhythm::SongChart rapid{{{.5,0,1},{193.0/384,0,1},{194.0/384,0,1},{1,0,2}}};
    rhythm::GuitarSession debounce(rapid);
    debounce.update({},0);debounce.update({1,false,true},.5);
    debounce.update({1,false,false},.501);
    check(!debounce.update({1,false,true},193.0/384),"same-direction strum inside five milliseconds is ignored");
    debounce.update({1,false,false},.504);
    check(debounce.update({1,false,true},194.0/384).has_value(),"same direction resumes at debounce deadline");
    debounce.reset();debounce.update({},0);debounce.update({1,false,true},.5);
    debounce.update({1,false,false},.501);
    check(debounce.update({1,true,false},193.0/384).has_value(),"alternate direction bypasses debounce");

    rhythm::PcStarPower sp;sp.reset(192);
    sp.awardPhrase();check(!sp.activate(0),"one phrase cannot activate SP");
    sp.awardPhrase();check(sp.activate(0),"half meter can activate SP");
    check(sp.remainingTicks()==2880,"PC half-meter is 15 beats at 192 PPQ");
    check(!sp.advance(7.5) && sp.fill()==.25f,"SP drains through chart beats");
    sp.awardPhrase();check(sp.fill()==.5f,"phrase during SP refills remaining meter");
    check(!sp.advance(15) && sp.active(),"active gain extends deployment");
    check(sp.advance(22.5) && !sp.active() && sp.fill()==0,"extended SP expires exactly");
    sp.reset(192);for(int i=0;i<6;++i)sp.awardPhrase();
    check(sp.fill()==1 && sp.remainingTicks()==5760,"full-meter cap");
    check(sp.activate(10) && !sp.advance(39.99) && sp.advance(40),"full SP lasts 30 beats");
    sp.reset(25);for(int i=0;i<4;++i)sp.awardPhrase();
    check(sp.remainingTicks()==752,"nearest-even rounding for 187.5 ticks");
    sp.reset(27);for(int i=0;i<4;++i)sp.awardPhrase();
    check(sp.remainingTicks()==808,"nearest-even rounds 202.5 down");
    sp.activate(0);sp.advance(1);const auto remaining=sp.remainingTicks();
    sp.advance(.5);check(sp.remainingTicks()==remaining,"clock correction cannot refund SP");
    std::puts("PASS: PC pull-offs, HOPO chain, taps, strum buffering, skip ordering and Star Power");
}
