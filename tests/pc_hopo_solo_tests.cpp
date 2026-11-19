#include "gameplay/pc_chart_rules.hpp"
#include "gameplay/pc_solo.hpp"
#include "gameplay/pc_fret_bonus.hpp"
#include <cstdio>
#include <cstdlib>
void check(bool v,const char* why){if(!v){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(){
 using namespace rhythm;
 check(pcHopoThreshold(192,0,false)==65 && pcHopoThreshold(480,0,false)==162,"PC truncates res*65/192");
 check(pcHopoThreshold(192,0,true)==97 && pcHopoThreshold(481,0,true)==241,"eighth-note threshold includes one tick");
 check(pcHopoThreshold(480,170,true)==170,"explicit frequency overrides eighth rule without rescaling");
 check(pcHopoThreshold(192,70000,false)==65535 && pcHopoThreshold(192,-1,false)==65,"INI frequency bounds and absent default");
 check(pcSoloBonus(49,100)==0 && pcSoloBonus(50,100)==0,"zero bonus below and exactly half accuracy");
 check(pcSoloBonus(3,4)==150 && pcSoloBonus(4,4)==400,"PC solo bonus per chord, no combo/SP scaling");
 check(pcSoloBonus(5,8)==120,"nearest-even rounding of 12.5 before multiplying ten");
 check(pcSoloBonus(7,8)==520,"nearest-even rounding of 52.5");
 check(pcSoloBonus(0,0)==0,"empty solo is safe");
 PcFretBonus fretBonus;
 check(fretBonus.award(true,false)==2,"separate PC fret bonus at full meter");
 check(fretBonus.award(false,true)==0,"broken run below x4 excludes bonus");
 fretBonus.ghosts(5);
 check(fretBonus.award(true,true)==1,"ghost deductions change bonus stage before recovery");
 fretBonus.reset();check(fretBonus.award(true,false,2)==4,"disjoint children award independently");
 PcSolo solo{2,5};
 check(solo.judge(1,true)==0 && solo.judged==0,"heads outside solo ignored");
 check(solo.judge(2,false)==0 && solo.judge(3,true)==0 && solo.judge(4,true)==0,"missed opening still starts solo accounting");
 check(solo.judge(5,true)==150 && solo.awarded,"last successful head pays bonus immediately");
 check(solo.judge(5,true)==0,"bonus cannot be paid twice");
 solo.reset();solo.judge(2,true);solo.judge(3,true);solo.judge(4,true);
 check(solo.judge(5,false)==150,"missed last head still settles earned bonus");
 solo.reset(false);check(solo.judge(5,true)==0 && !solo.awarded,"practice seek cannot invent full solo success");
 PcSolo drums{0,3};drums.judge(0,true,false);drums.judge(1,true,false);
 check(drums.judge(3,true,false,true)==0 && !drums.awarded,"last drum child waits for pending root");
 check(drums.judge(2,true,true,true)==400,"final drum root settles all four solo gems");
 drums.reset();drums.judge(0,true,false);drums.judge(1,true,false);drums.judge(2,true,false,true);
 check(drums.judge(3,false,true,true)==150,"missed final sibling settles partial drum bonus");
 std::puts("PASS: PC per-song HOPO thresholds, solo cutoff, chord counting and midpoint rounding");
}
