#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rhythm {
// BaseEngine ctor 0x1820F3D80: the double at 0x183153E90 is 7.5,
// not 30. Round uses nearest/even (System.Math.Round), then four units
// form a full meter. Award 0x1820F13F0 and update 0x1820F2B90 retain
// gains during deployment. The caller supplies the PC measure-clock ticks
// (see pc_star_power_clock.hpp), independently of raw sustain/chart ticks.
class PcStarPower {
public:
    void reset(int resolution) noexcept {
        resolution_=std::max(resolution,1);
        const double ticks=resolution_*7.5;
        const auto whole=static_cast<std::int64_t>(ticks);
        phraseTicks_=whole+((ticks-whole>0.5 || (ticks-whole==0.5 && (whole&1)))?1:0);
        remaining_=lastTick_=startTick_=endTick_=0;startTime_=0;active_=false;
    }
    bool advance(double beat) noexcept {
        if(!std::isfinite(beat) || !active_) return false;
        return advanceTicks(static_cast<std::int64_t>(beat*resolution_));
    }
    bool advanceTicks(std::int64_t tick,bool permitExpiry=true) noexcept {
        if(!active_) return false;
        // Audio correction must not refund already consumed ticks.
        if(tick>lastTick_) {
            remaining_=std::max<std::int64_t>(0,remaining_-(tick-lastTick_));
            lastTick_=tick;
        }
        if(remaining_!=0 || !permitExpiry) return false;
        active_=false;
        return true;
    }
    [[nodiscard]] bool available() const noexcept {return !active_ && remaining_>=2*phraseTicks_;}
    bool activate(double beat) noexcept {
        return activatePosition(beat*resolution_,0);
    }
    bool activatePosition(double position,double time) noexcept {
        if(!std::isfinite(position) || !std::isfinite(time) || active_ || remaining_<2*phraseTicks_) return false;
        lastTick_=startTick_=static_cast<std::int64_t>(position);
        startTime_=time;endTick_=startTick_+remaining_;
        active_=true;
        return true;
    }
    void awardPhrase() noexcept {
        const auto before=remaining_;remaining_=std::min(4*phraseTicks_,remaining_+phraseTicks_);
        if(active_)endTick_+=remaining_-before;
    }
    [[nodiscard]] std::int64_t expirationTick() const noexcept {return active_?endTick_:lastTick_+remaining_;}
    [[nodiscard]] std::int64_t activationTick() const noexcept {return startTick_;}
    [[nodiscard]] double activationTime() const noexcept {return startTime_;}
    [[nodiscard]] bool active() const noexcept {return active_;}
    [[nodiscard]] float fill() const noexcept {return float(remaining_)/float(4*phraseTicks_);}
    [[nodiscard]] std::int64_t remainingTicks() const noexcept {return remaining_;}
private:
    int resolution_{192};
    std::int64_t phraseTicks_{1440},remaining_{},lastTick_{},startTick_{},endTick_{};
    double startTime_{};
    bool active_{};
};
}
