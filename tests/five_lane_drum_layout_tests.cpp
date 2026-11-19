#include "../examples/cloneheronative/gameplay/pc_drums_visuals.hpp"
#include "../examples/cloneheronative/gameplay/pc_drums.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
    // The five physical kit voices must remain distinct after PC conversion.
    constexpr unsigned lanes[]{1,5,3,7,4};
    unsigned used=0;
    for(unsigned slot=0;slot<5;++slot){
        const unsigned pad=rhythm::fiveLaneDrumPad(lanes[slot]);
        assert(pad==slot+1);used|=1u<<pad;
        assert(rhythm::fiveLaneDrumSource(pad)<4);
        const float x=rhythm::drumPadX(slot,5,-.357f);
        assert(std::abs(x)<=.357001f);
        if(slot)assert(x>rhythm::drumPadX(slot-1,5,-.357f));
    }
    assert(used==62 && rhythm::fiveLaneDrumPad(0)==0);
    assert(rhythm::fiveLaneDrumCymbal(5) && rhythm::fiveLaneDrumCymbal(7));
    assert(!rhythm::fiveLaneDrumCymbal(1) && !rhythm::fiveLaneDrumCymbal(4));
    // Chord conversion stays the PC engine's rule; the view cannot change masks.
    assert(rhythm::pc_drums::fiveLaneChartMask(4,4)==8);
    assert(rhythm::pc_drums::fiveLaneChartMask(4,12)==2);
    assert(rhythm::pc_drums::fiveLaneChartMask(64,64)==128);
    assert(rhythm::pc_drums::fiveLaneChartMask(64,80)==32);
    std::puts("PASS five distinct kit voices, cymbal identities and bounded lane spacing");
}
