#include "gameplay/guitar_session.hpp"
#include "gameplay/pc_sustain.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>

void check(bool value,const char* label) {
    if(!value) {std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}
}

// Synthetic replay with a surviving disjoint extended tail, tenth head at tick 140, SP
// deployment at 280 and expiration at 3160. Timeline changes are processed
// before that timestamp's tails, as in PC 0x1820F0840. Tick 140 earns 2x,
// tick 280 earns 4x and expiration tick 3160 earns ordinary 2x.
uint64_t replay(int frameTicks) {
    rhythm::PcStarPower sp;sp.reset(192);sp.awardPhrase();sp.awardPhrase();
    unsigned streak=9,best=9,paid=0;uint64_t score=0;
    auto advance=[&](int tick) {
        rhythm::advancePcSustainScore(sp,tick,[&](int64_t through,bool active) {
            const auto due=rhythm::pc_scoring::sustainPoints(4000,through,192);
            if(due>paid) {
                score+=(due-paid)*rhythm::pc_scoring::multiplier(streak,active);paid=due;
            }
        });
    };
    const int boundaries[]{140,280,3160};unsigned event=0;
    for(int frame=frameTicks;frame<4000+frameTicks;frame+=frameTicks) {
        const int through=std::min(frame,4000);
        while(event<3 && boundaries[event]<=through) {
            const int t=boundaries[event];advance(t-1);sp.advanceTicks(t);
            if(event==0) score+=rhythm::pc_scoring::hit(1,streak,best,false);
            else if(event==1) check(sp.activate(double(t)/192),"activate at scheduled tick");
            advance(t);++event;
        }
        advance(through);
    }
    return score;
}
int main() {
    check(rhythm::GuitarSession::sustainFretsMatch({0,1,2},3),"normal single tail allows lower fret anchoring");
    check(!rhythm::GuitarSession::sustainFretsMatch({0,1,2},6),"higher fret breaks normal single tail");
    check(!rhythm::GuitarSession::sustainFretsMatch({0,1,6,false,false,true},2),"ordinary chord tail needs full chord");
    rhythm::PcSustainGrace grace;
    check(grace.scoring(),"new tail scores");
    grace.sample(1,false);check(!grace.scoring() && !grace.dropped(),"release freezes without immediate drop");
    grace.sample(1.004,true);check(grace.scoring(),"recover before five milliseconds");
    grace.sample(2,false);grace.advance(2.005);
    check(grace.dropped(),"scheduled release expires without another input");
    grace.sample(2.006,true);check(!grace.scoring(),"late repress cannot resurrect tail");
    grace={};grace.sample(1,false);grace.sample(1.005,true);
    check(grace.scoring(),"PC accepts recovery exactly at the release deadline");
    grace={};grace.sample(1,false);grace.advance(1.005,false);grace.sample(1.005,true);
    check(grace.scoring(),"pre-input settlement leaves exact-deadline recovery available");
    grace={};grace.sample(1,false);grace.sample(1.005000000001,true);
    check(grace.dropped(),"recovery after the exact deadline cannot resurrect a tail");
    check(rhythm::pc_scoring::sustainPoints(28,0,192)==4,"short tail completes at head inside quarter beat");
    check(rhythm::pc_scoring::sustainPoints(28,-1,192)==0,"early hit does not score tail before chart tick");
    // 19 points at 1x, 20 at 2x, 412 at 4x, 121 at 2x; head 100.
    check(replay(1)==2049,"known score across multiplier and SP boundaries");
    for(int step:{3,6,13,64,4096}) check(replay(step)==2049,"score independent of frame cadence/stall");

    rhythm::SongChart chart{{{1,0,1},{1.03,0,2}}};
    rhythm::GuitarSession session(chart);
    session.update({2,false,true},1.03);
    check(session.events().size()==2,"skipped miss and successful head both exposed");
    check(session.events()[0].kind==rhythm::GuitarEventKind::Miss &&
        session.events()[1].kind==rhythm::GuitarEventKind::Hit,"miss precedes recovering head");
    session.reset();session.update({4,false,true},.98);session.update({},1.2);
    const auto& events=session.events();
    check(events.size()==2 && events[0].kind==rhythm::GuitarEventKind::Miss &&
        events[1].kind==rhythm::GuitarEventKind::Miss && events[0].time<events[1].time,
        "pre-play buffer is ignored and expired heads retain chronological deadlines");
    session.reset();session.updateBot(2);
    check(session.events().size()==2 && session.events()[0].time==1 &&
        session.events()[1].time==1.03,"bot retains individual timestamps across a stalled frame");
    std::puts("PASS: PC sustain release, chronological judgements and scoring transition replay");
}
