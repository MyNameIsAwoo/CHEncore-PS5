#include "gameplay/pc_sustain_rules.hpp"
#include "gameplay/guitar_session.hpp"
#include <cstdio>
#include <cstdlib>
void check(bool v,const char* why){if(!v){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(){
 using namespace rhythm;
 std::vector<PcSustainGem> retrigger{{0,384,1},{96,0,1},{352,0,1}};
 normalizePcSustainLengths(retrigger);
 check(retrigger[0].length==96 && retrigger[1].length==256 && retrigger[2].length==32,"same-fret overlap truncates and carries the original end to later gems");
 std::vector<PcSustainGem> chord{{0,192,1},{0,192,2},{192,192,4}};
 classifyPcSustains(chord);
 check(!chord[0].disjoint && !chord[0].extended && !chord[2].extended,"ordinary equal tails touching next sustained head are not extended");
 std::vector<PcSustainGem> split{{0,192,1},{0,96,2},{120,0,4},{192,0,8}};
 classifyPcSustains(split);
 check(split[0].disjoint && split[1].disjoint,"different chord lengths retain disjoint children");
 check(split[0].extended && split[1].extended && split[2].extended && split[3].extended,"overlap flags include both heads and zero-length boundary head");
 check(!pcHeadStopsTail(true,4,1) && pcHeadStopsTail(true,1,1) && pcHeadStopsTail(false,4,1),"new head retains unrelated extended tail only");
 check(pcExtendedTailHeld(1,5) && !pcExtendedTailHeld(1,4),"extended gem independently tracks fret");
 check(pcExtendedTailHeld(32,31),"source Open extended special case");
 SongChart track{{{1,0,1,false,false,false,false,true}}};GuitarSession input(track);
 input.setSustainMasks(16);
 check(input.update({17,false,true},1).has_value(),"higher held extended sustain does not block lower new head");
 input.reset();
 check(!input.update({17,false,true},1),"unrelated higher fret still rejects ordinary mask without sustain");
 input.reset();input.setSustainMasks(0,16);
 check(input.update({17,false,true},1).has_value(),"completed extended sustain fallback");
 std::puts("PASS: PC disjoint grouping, overlap flags, tail retirement and extended input masks");
}
