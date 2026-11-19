#pragma once

namespace rhythm {
enum class GuitarHead { Strum,Hopo,Tap };
// .chart loader 0x1820C3020 / 0x1820C4CC0: positive INI frequency
// is an absolute tick threshold; default floor(res*65/192), eighth=floor(res/2)+1.
constexpr int pcHopoThreshold(int resolution,int frequency,bool eighth) noexcept {
    if(frequency>0) return frequency>65535?65535:frequency;
    return eighth ? resolution/2+1 : static_cast<int>(static_cast<long long>(resolution)*65/192);
}
// Natural classifier 0x182170210: Open participates as a single, a chord
// cannot be a natural HOPO, repeated singles strum, and the threshold is
// inclusive. Source masks reserve bit zero for Open. The chart config may
// additionally suppress a chord-to-single transition contained in that chord.
constexpr bool naturalGuitarHopo(unsigned current, unsigned previous,
    long long gapTicks, long long thresholdTicks, bool blockChordSubset=false) noexcept {
    const bool single=current && !(current&(current-1));
    const bool previousChord=(previous&(previous-1))!=0;
    if(!single || !previous || gapTicks<=0 || gapTicks>thresholdTicks) return false;
    if(!previousChord && current==previous) return false;
    return !(blockChordSubset && previousChord && (previous&current)==current);
}
// PC .chart N5 maps to Flip (0x1820E44E0). Classification 0x182170660
// toggles Strum/Hopo once per head; Tap replaces both flags.
constexpr GuitarHead chartGuitarHead(bool naturalHopo,bool flip,bool tap) noexcept {
    return tap?GuitarHead::Tap:(naturalHopo!=flip?GuitarHead::Hopo:GuitarHead::Strum);
}
// Native selection stores a compact mask: AllStrums, AllHopos, AllTaps,
// AllOpens, HoposToTaps in bits 0..4. PC conversion 0x1820B9A80 chooses
// one of these branches in this order, rather than applying all of them.
constexpr GuitarHead modifiedGuitarHead(GuitarHead head,unsigned nativeMask) noexcept {
    if(nativeMask&8u) return GuitarHead::Strum;
    if(nativeMask&4u) return GuitarHead::Tap;
    if(nativeMask&2u) return GuitarHead::Hopo;
    if(nativeMask&1u) return GuitarHead::Strum;
    if((nativeMask&16u) && head==GuitarHead::Hopo) return GuitarHead::Tap;
    return head;
}
constexpr bool allStrumsRequired(unsigned nativeMask) noexcept {
    return (nativeMask&1u) && !(nativeMask&14u);
}
}
