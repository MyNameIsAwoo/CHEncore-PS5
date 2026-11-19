#include "gameplay/pc_beats.hpp"
#include <cstdio>
#include <cstdlib>
#include <utility>
std::vector<std::pair<long,unsigned>> grid(int resolution,long end,std::vector<rhythm::BeatSignature> signatures) {
    std::vector<std::pair<long,unsigned>> out;
    rhythm::pcBeatTicks(resolution,end,signatures,[&](long tick,unsigned kind){out.emplace_back(tick,kind);});return out;
}
void check(bool value) {if(!value){std::fputs("FAIL: PC beat grid\n",stderr);std::exit(1);}}
int main() {
 auto four=grid(192,768,{{0,4,4}});
 check(four==std::vector<std::pair<long,unsigned>>{{0,0},{192,1},{384,1},{576,1},{768,0}});
 auto six=grid(192,576,{{0,6,8}});
 check(six==std::vector<std::pair<long,unsigned>>{{0,0},{96,2},{192,1},{288,2},{384,1},{480,2},{576,0}});
 auto change=grid(192,1344,{{0,4,4},{768,3,4}});
 check(change[4]==std::pair<long,unsigned>{768,0} && change.back()==std::pair<long,unsigned>{1344,0});
 check(grid(0,100,{{0,4,4}}).empty());
 std::puts("PASS: PC 4/4, 6/8, signature-change and invalid-resolution beat grids");
}
