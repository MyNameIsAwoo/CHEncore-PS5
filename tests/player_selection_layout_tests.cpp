#include "../examples/cloneheronative/gameplay/play_menus.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
    // All physical-controller combinations, including holes left by drop-outs.
    for(unsigned mask=1;mask<16;++mask) {
        const int count=std::popcount(mask);int ordinal=0;float first=0,last=0,previous=0;
        for(int player=0;player<4;++player) if(mask&(1u<<player)) {
            const float left=rhythm::play_menu::cardLeft(ordinal,count);
            assert(left>=0 && left+450<=1920);
            if(ordinal) assert(std::abs(left-previous-450-20)<.001f);
            else first=left;
            previous=last=left;++ordinal;
        }
        assert(std::abs((first+last+450)*.5f-960)<.001f);
    }
    std::puts("PASS all 15 player combinations stay centered with 20px gaps");
}
