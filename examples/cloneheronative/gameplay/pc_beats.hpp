#pragma once
#include <algorithm>
#include <vector>
namespace rhythm {
struct BeatSignature {long tick;unsigned numerator,denominator;};
// Ghidra 0x182180FF0: denominator subdivisions; restart measure counting on
// signature changes. Denominators above four have weak subdivisions.
template<class Submit> void pcBeatTicks(int resolution,long lastTick,const std::vector<BeatSignature>& signatures,Submit submit) {
    if(resolution<=0 || lastTick<0 || signatures.empty()) return;
    size_t active=0;unsigned count=0;
    for(long tick=0;tick<=lastTick;) {
        const size_t previous=active;
        while(active+1<signatures.size() && signatures[active+1].tick<=tick) ++active;
        if(active!=previous) count=0;
        const auto& s=signatures[active];
        if(!s.numerator || !s.denominator) return;
        if(count==s.numerator) count=0;
        const unsigned kind=count==0?0u:(s.denominator<=4 || count%std::max(1u,s.denominator/4)==0?1u:2u);
        submit(tick,kind);
        const long step=std::max(1L,static_cast<long>(resolution)*4/s.denominator);
        if(lastTick-tick<step) break;
        tick+=step;++count;
    }
}
}
