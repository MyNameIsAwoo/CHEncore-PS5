#include "gameplay/pc_scoring.hpp"
#include <cstdio>
#include <cstdlib>
void check(bool value,const char* label) {if(!value){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}}
int main() {
 using namespace rhythm::pc_scoring;
 check(healthAfter(1.0f,true,false)==1.0f,"full health does not gain again");
 check(healthAfter(.999f,true,true)>1.0f,"source health retains overshoot");
 check(healthAfter(.02f,false,false)<0,"miss and overstrum can reach failure");
 unsigned streak=0,best=0;uint64_t score=0;
 // Golden event sequence from recovered hit ordering, not per-gem streak.
 for(int i=0;i<9;++i) score+=hit(1,streak,best,false);
 check(score==450 && streak==9,"first nine single notes");
 score+=hit(3,streak,best,false);
 check(score==750 && streak==10 && best==10,"tenth chord earns 2x for every gem");
 check(hit(5,streak,best,false)==500 && streak==11,"five gems advance streak once");
 check(multiplier(19,false)==2 && multiplier(20,false)==3 && multiplier(30,false)==4,"20/30 chord boundaries");
 check(multiplier(0,true)==2 && multiplier(20,true)==6 && multiplier(300,true)==8,"SP doubles all multiplier tiers");
 streak=0;check(multiplier(streak,true)==2 && hit(2,streak,best,true)==200,"broken streak during SP");
 check(sustainInterval(192)==7 && sustainInterval(480)==19,"integer sustain cadence");
 check(sustainPoints(192,140,192)==20,"tail before end grace");
 check(sustainPoints(192,144,192)==28,"quarter-beat grace awards remainder");
 check(2*sustainPoints(192,144,192)==56,"two held chord tails score independently");
 check(sustainPoints(192,0,192)==0,"no tail award before head");
 check(!countsTowardBase(63,192) && countsTowardBase(64,192),"denominator third-beat cutoff");
 check(baseScore(3,384,192)==200,"base uses floating 25 points per beat once over sum");
 check(baseScore(2,65+65,192)==117,"base ceils total tail points once");
 check(baseScore(1,57,57)==76 && baseScore(1,113,113)==76,"PC preserves above-integer IEEE quotient before ceil");
 std::puts("PASS: PC chord streak, multiplier, SP, per-gem sustain and denominator golden cases");
}
