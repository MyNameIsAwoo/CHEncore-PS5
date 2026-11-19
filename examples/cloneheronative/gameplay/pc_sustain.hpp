#pragma once
#include "pc_scoring.hpp"
#include "pc_star_power.hpp"
#include <optional>

namespace rhythm {
// GuitarEngine 0x180FD0C20 / 0x180FCE040: first release scores through
// that input update, then scoring freezes. Recovery before the scheduled
// deadline catches up. The default setting is 5 / 1000 seconds, from
// 0x1800B2D80 (+0x218), 0x1820D95F0 and double 0x183077338.
class PcSustainGrace {
public:
    void advance(double time,bool includeEndpoint=true) noexcept {
        if(deadline_ && (time>*deadline_ || (includeEndpoint && time==*deadline_))) dropped_=true;
    }
    void sample(double time,bool held) noexcept {
        // 0x180FD0C20 clears a pending release from the sampled held state
        // before testing its deadline. Exact-deadline recovery still succeeds.
        if(!dropped_ && held && deadline_ && time<=*deadline_)deadline_.reset();
        advance(time);
        if(dropped_) return;
        if(held) deadline_.reset();
        else if(!deadline_) deadline_=time+0.005;
    }
    [[nodiscard]] bool scoring() const noexcept {return !dropped_ && !deadline_;}
    [[nodiscard]] bool dropped() const noexcept {return dropped_;}
private:
    std::optional<double> deadline_;
    bool dropped_{};
};

// BaseEngine 0x1820F0840 updates SP before guitar judgement/tails. Split
// a batch at expiration so its final tick uses the ordinary multiplier.
// Caller retains each gem's awarded count; repeated boundaries pay nothing.
template<class Award>
bool advancePcSustainScore(PcStarPower& sp,std::int64_t tick,Award&& award) {
    bool released=false;
    if(sp.active() && sp.expirationTick()<=tick) {
        const auto end=sp.expirationTick();
        award(end-1,true);
        released=sp.advanceTicks(end);
    }
    award(tick,sp.active());
    released=sp.advanceTicks(tick)||released;
    return released;
}
}
