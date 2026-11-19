#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rhythm::pc_scoring {
// BaseEngine 0x1820F1290 keeps raw health (including a slight overshoot
// above one); UI normalization is separate. Every streak break loses health.
constexpr float healthAfter(float health,bool hit,bool active) noexcept {
    if(!hit) return health-0.0277f;
    if(health>=1.0f) return health;
    return health+(active?0.045f:0.0069f);
}
// Ghidra BaseEngine 0x1820F1660; hit ordering 0x1820F2790 and
// guitar hit 0x180FCCED8: advance chord streak, update multiplier, award gems.
constexpr unsigned multiplier(unsigned streak, bool active) noexcept {
    return std::min(4u, 1u + streak / 10u) * (active ? 2u : 1u);
}
inline uint64_t hit(unsigned gems, unsigned& streak, unsigned& best, bool active) noexcept {
    ++streak;
    best=std::max(best,streak);
    return uint64_t{50} * gems * multiplier(streak,active);
}
constexpr unsigned sustainInterval(int resolution) noexcept {
    return static_cast<unsigned>(std::max(resolution,25)/25);
}
// 0x180FCE260: each held gem has its own remaining tick count. Complete
// and ceil its final fractional point within resolution/4 ticks of the end.
inline unsigned sustainPoints(double lengthTicks, double elapsedTicks, int resolution) noexcept {
    if(lengthTicks<=0 || elapsedTicks<0) return 0;
    const double interval=sustainInterval(resolution);
    if(elapsedTicks>=lengthTicks-std::max(resolution,0)/4)
        return static_cast<unsigned>(std::ceil(lengthTicks/interval-1e-9));
    return static_cast<unsigned>(std::floor(elapsedTicks/interval+1e-9));
}
// 0x1820F0B30: maximum-score denominator uses the sum of qualifying tail
// ticks and a floating resolution/25 divisor, independent of award cadence.
inline bool countsTowardBase(double lengthTicks,int resolution) noexcept {
    return lengthTicks>=std::ceil(resolution/3.0);
}
inline uint64_t baseScore(unsigned gems,double qualifyingTailTicks,int resolution) noexcept {
    // The original ceil keeps IEEE rounding, including quotients just above
    // an integer (57/(57/25.0) produces 26, not 25). Do not subtract an epsilon.
    return uint64_t{50}*gems+static_cast<uint64_t>(std::ceil(qualifyingTailTicks/(std::max(resolution,1)/25.0)));
}
}
