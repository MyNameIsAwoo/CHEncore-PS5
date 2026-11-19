#pragma once
#include "song_pipeline.hpp"
#include "pc_six_fret.hpp"
#include "pc_fret_bonus.hpp"
#include "pc_timing.hpp"
#include <algorithm>
#include <cmath>
#include <array>
#include <bit>
#include <span>
#include <limits>

namespace rhythm {
// Five-bit mask: green=1, red=2, yellow=4, blue=8, orange=16.
struct GuitarButtons {
    std::uint16_t frets = 0;
    bool strumUp = false;
    bool strumDown = false;
};

// Events carry their logical time rather than the frame endpoint. The adapter
// can settle sustained scoring before each change of streak/Star Power.
enum class GuitarEventKind { Hit, Miss, Overstrum, Ghost };
struct GuitarEvent {
    GuitarEventKind kind;
    std::size_t noteIndex;
    Seconds time;
    unsigned fretBonus{};
};
class GuitarSession {
public:
    explicit GuitarSession(const SongChart& chart) : chart_(chart), evaluator_(chart) {}
    // Park a complete player's runtime without copying notes or rebinding chart
    // references. The per-instance window lambda must retain its own `this`.
    void swapRuntimeState(GuitarSession& other) noexcept {
        evaluator_.swapRuntimeState(other.evaluator_);
        std::swap(events_,other.events_);
        std::swap(activeSustainFrets_,other.activeSustainFrets_);
        std::swap(completedSustainFrets_,other.completedSustainFrets_);
        std::swap(previous_,other.previous_);
        std::swap(hitWindow_,other.hitWindow_);
        std::swap(gamepadMode_,other.gamepadMode_);
        std::swap(classicGamepad_,other.classicGamepad_);
        std::swap(autoStrum_,other.autoStrum_);
        std::swap(allStrums_,other.allStrums_);
        std::swap(noFretGhosting_,other.noFretGhosting_);
        std::swap(precisionMode_,other.precisionMode_);
        std::swap(exactFrets_,other.exactFrets_);
        std::swap(hits_,other.hits_);
        std::swap(misses_,other.misses_);
        std::swap(overstrums_,other.overstrums_);
        std::swap(botCursor_,other.botCursor_);
        std::swap(streak_,other.streak_);
        std::swap(chainBroken_,other.chainBroken_);
        std::swap(fretBonus_,other.fretBonus_);
        std::swap(lastUpdate_,other.lastUpdate_);
        std::swap(streakBreaks_,other.streakBreaks_);
        std::swap(ghostCount_,other.ghostCount_);
        std::swap(usedFrets_,other.usedFrets_);
        std::swap(unusedRelease_,other.unusedRelease_);
        std::swap(fretValidUntil_,other.fretValidUntil_);
        std::swap(strumUntil_,other.strumUntil_);
        std::swap(hopoStrumUntil_,other.hopoStrumUntil_);
        std::swap(lastStrum_,other.lastStrum_);
        std::swap(lastStrumUp_,other.lastStrumUp_);
        std::swap(strumDebounce_,other.strumDebounce_);
        std::swap(eligibleFront_,other.eligibleFront_);
    }
    void reset() {
        evaluator_.reset(); previous_ = {}; hits_ = misses_ = overstrums_ = 0;
        botCursor_=0;events_.clear();activeSustainFrets_=completedSustainFrets_=0;
        chainBroken_=false;streak_=0;
        fretBonus_.reset();lastUpdate_.reset();lastStrum_.reset();
        streakBreaks_=ghostCount_=0;usedFrets_=0;
        for(auto& release:unusedRelease_) release.reset();
        fretValidUntil_.reset();strumUntil_.reset();hopoStrumUntil_.reset();
        eligibleFront_.reset();
    }
    void beginSection(Seconds startTime) {
        reset();
        evaluator_.ignoreNotesBefore(startTime);
    }
    void setHitWindow(Seconds halfWidth) {
        hitWindow_ = makeHitWindow(halfWidth, false);

    }
    // PC 0x180FC9DB0 compares elapsed time only for the same strum direction.
    void setStrumDebounce(Seconds seconds) noexcept {
        strumDebounce_=std::isfinite(seconds)?std::clamp(seconds,0.0,0.05):0.005;
    }
    void setPrecisionMode(bool enabled) noexcept {precisionMode_=enabled;}
    [[nodiscard]] HitWindow noteWindow(std::size_t index) const noexcept {
        return precisionMode_?pcPrecisionWindow(chart_,index):hitWindow_;
    }
    void setGamepadMode(bool enabled,bool classic=false) {gamepadMode_=enabled;classicGamepad_=classic;}
    void setAutoStrum(bool enabled) noexcept {autoStrum_=enabled;}
    // PC "All Strums": HOPO/tap chart notes also require a strum edge.
    void setAllStrums(bool enabled) { allStrums_ = enabled; }
    // PC NoFretGhosting changes the unused-fret release threshold, not the
    // lower-fret anchoring comparison (0x180FCD9E0 / 0x180FD2910).
    void setNoFretGhosting(bool enabled) { noFretGhosting_ = enabled; }
    [[nodiscard]] unsigned ghostCount() const noexcept {return static_cast<unsigned>(ghostCount_);}
    [[nodiscard]] unsigned fretCharge() const noexcept {return fretBonus_.charge();}
    void setExactFrets(bool enabled) {exactFrets_=enabled;}
    void setSustainMasks(std::uint16_t active,std::uint16_t completed=0) noexcept {
        activeSustainFrets_=active;completedSustainFrets_=completed;
    }
    // Normal (non-ExtendedSustain) tails use the same PC fret comparison.
    static bool sustainFretsMatch(const Note& note,std::uint16_t frets,ChartKind kind=ChartKind::FiveFret) {
        if(kind==ChartKind::SixFret)return pcSixFretMatch((unsigned(note.frets)<<1)|(note.open||!note.frets?1u:0u),unsigned(frets)<<1,note.chord);
        return matchesPcFretMask(note,frets,false);
    }
    // Prime on connection/resume so a held strum does not generate a hit.
    void prime(const GuitarButtons& buttons) {
        lastUpdate_.reset();
        eligibleFront_.reset();
        previous_ = buttons;lastStrum_.reset();
        fretValidUntil_.reset();strumUntil_.reset();hopoStrumUntil_.reset();
        for(auto& release:unusedRelease_) release.reset();
    }
    using StepObserver=std::function<void(const GuitarButtons&,Seconds,bool,std::span<const GuitarEvent>)>;
    std::optional<HitResult> update(const GuitarButtons& input, Seconds time,const StepObserver& observer={}) {
        events_.clear();
        if(!std::isfinite(time)) return std::nullopt;
        std::optional<HitResult> result;
        const auto step=[&](const GuitarButtons& state,Seconds at) {
            if(observer) observer(state,at,true,{});
            const auto begin=events_.size();
            auto hit=updateStep(state,at);
            if(observer) observer(state,at,false,std::span<const GuitarEvent>(events_).subspan(begin));
            return hit;
        };
        // 0x1820F0840 / 0x1820F3390: collect and drain scheduled boundaries,
        // then regenerate at most three times. Old input remains in force at
        // those times; the newly sampled state belongs only to the endpoint.
        if(lastUpdate_ && time>*lastUpdate_) for(unsigned pass=0;pass<3;++pass) {
            std::vector<Seconds> boundaries;
            bool refreshedOpenWindow=false;
            const auto add=[&](Seconds value) {
                if(std::isfinite(value) && *lastUpdate_<value && value<time) boundaries.push_back(value);
            };
            const auto pending=evaluator_.nextPendingNote();
            const auto horizon=std::nextafter(time+maximumEarlyWindow(),INFINITY);
            for(size_t i=pending.value_or(chart_.notes.size());i<chart_.notes.size();++i) {
                if(chart_.notes[i].time>horizon)break;
                if(evaluator_.state(i)!=NoteState::Pending)continue;
                const auto& note=chart_.notes[i];const auto window=windowFor_(i,note);
                auto early=note.time-window.early;
                if(note.time-early>window.early) early=std::nextafter(early,INFINITY);
                // Base scheduler 0x1820F2DD0 refreshes an already-open
                // candidate window immediately after the previous update.
                // That substep still uses the previous held input; replacing
                // it with the new endpoint state can delay a following HOPO.
                if(!refreshedOpenWindow && early<=*lastUpdate_ &&
                   *lastUpdate_<window.lateDeadline(note.time)) {
                    add(std::nextafter(*lastUpdate_,INFINITY));
                    refreshedOpenWindow=true;
                }
                add(early);add(window.lateDeadline(note.time));
            }
            for(const auto& deadline:{strumUntil_,fretValidUntil_,hopoStrumUntil_}) if(deadline) add(*deadline);
            for(const auto& deadline:unusedRelease_) if(deadline) add(*deadline);
            std::sort(boundaries.begin(),boundaries.end());
            boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
            if(boundaries.empty()) break;
            for(const auto boundary:boundaries) {
                if(auto hit=step(previous_,boundary)) result=hit;
                lastUpdate_=boundary;
            }
        }
        if(auto hit=step(input,time)) result=hit;
        lastUpdate_=time;
        std::stable_sort(events_.begin(),events_.end(),[](const auto& a,const auto& b){return a.time<b.time;});
        return result;
    }
private:
    std::optional<HitResult> updateStep(const GuitarButtons& input,Seconds time) {
        // Guitar timer update 0x180FCAA30 precedes candidate expiration.
        // Its overstrum guard sees the pre-miss streak (0x180FC8E20).
        if(strumUntil_ && time>=*strumUntil_) {
            recordOverstrum(*strumUntil_);
            strumUntil_.reset();
        }
        // BaseEngine 0x1820F1AD0 expires heads before guitar judgement.
        const auto expired=evaluator_.expireNotes(time,windowFor_);
        misses_+=expired.size();
        for(const auto index:expired)
            events_.push_back({GuitarEventKind::Miss,index,
                windowFor_(index,chart_.notes[index]).lateDeadline(chart_.notes[index].time)});
        if(!expired.empty()) {ghostCount_=0;fretValidUntil_.reset();breakChain();}
        if(fretValidUntil_ && time>=*fretValidUntil_) fretValidUntil_.reset();
        const auto eligible=evaluator_.eligibleNotes(time,windowFor_,maximumEarlyWindow());
        eligibleFront_=eligible.empty()?std::nullopt:std::optional<std::size_t>{eligible.front()};
        bool strummed = (input.strumUp && !previous_.strumUp) ||
                              (input.strumDown && !previous_.strumDown);
        const bool physicalStrum=strummed;
        const bool fretPressed = (input.frets & ~previous_.frets) != 0;
        const bool fretChanged=input.frets!=previous_.frets;
        const auto pending=evaluator_.nextPendingNote();
        // 0x1820F1AD0 leaves the current window on the last inspected
        // candidate, including the first head beyond its lookahead. The
        // guitar timing refresh (0x180FCB010) uses its full width / 3.
        Seconds precisionInput=0.041999999999999996;
        if(precisionMode_ && pending) for(size_t i=*pending;i<chart_.notes.size();++i) {
            const auto& candidate=chart_.notes[i];const auto w=windowFor_(i,candidate);
            precisionInput=(w.early+w.late)/3.0;
            const auto ahead=candidate.time-time;
            if(ahead>w.early && (!(candidate.tap || candidate.hopo) || ahead>w.early*3)) break;
        }
        if(gamepadMode_ && pending && (fretPressed || (!classicGamepad_ && fretChanged && input.frets==0 && chart_.notes[*pending].open))) strummed=true;
        // 0x180FC9DB0: debounce only a repeated direction; alternate strums
        // remain eligible even inside the default 5ms input threshold.
        if(strummed) {
            const bool up=input.strumUp;
            if(lastStrum_ && up==lastStrumUp_ && time<*lastStrum_+((gamepadMode_ || autoStrum_)?0.0:strumDebounce_)) strummed=false;
            else {lastStrum_=time;lastStrumUp_=up;}
        }
        // 0x180FC9DB0 arms the fret-valid timer on accepted strums too.
        // A protected HOPO re-strum returns before arming that timer.
        if(strummed && (!hopoStrumUntil_ || time>=*hopoStrumUntil_)) {
            fretValidUntil_=precisionMode_?time+precisionInput:std::numeric_limits<Seconds>::infinity();
            // A fresh strum consumes an existing buffer before judgement,
            // even when this new input subsequently hits (0x180FC9DB0).
            if(physicalStrum && strumUntil_) {recordOverstrum(time);strumUntil_.reset();}
        }
        // 0x180FC7A00 / 0x180FCA710: an unused fret must remain released
        // for 7ms before it counts as a ghost. Previously used hit frets do
        // not count, and re-pressing a fret cancels its pending release.
        unsigned releasedGhosts=0;
        for(unsigned lane=0;lane<(chart_.kind==ChartKind::SixFret?6u:5u);++lane) {
            const unsigned bit=1u<<lane;
            if(unusedRelease_[lane] && time>=*unusedRelease_[lane]) {
                if(!(usedFrets_&bit)) releasedGhosts|=bit;
                unusedRelease_[lane].reset();
            }
        }
        if(releasedGhosts) {
            const auto next=evaluator_.nextPendingNote();
            // 0x180FD1FB0 ignores releases before play, after the chart, for a
            // pending strum, and for a HOPO whose recovery streak is already zero.
            if(next && !chart_.notes.empty() && time>=chart_.notes.front().time) {
                const auto& note=chart_.notes[*next];
                if((note.hopo || note.tap) && !(note.hopo && streak_==0)) {
                    const unsigned count=std::popcount(releasedGhosts);
                    ghostCount_+=count;
                    // Base candidate collection 0x1820F1AD0 includes legato
                    // heads up to three half-windows ahead for this meter.
                    for(size_t i=*next;i<chart_.notes.size();++i) {
                        const auto& candidate=chart_.notes[i];
                        const auto window=windowFor_(i,candidate);
                        if((candidate.hopo || candidate.tap) && candidate.time-time<=window.early*3 &&
                           time<=candidate.time+window.late && evaluator_.state(i)==NoteState::Pending) {
                            fretBonus_.ghosts(count);break;
                        }
                    }
                }
            }
        }
        usedFrets_&=static_cast<std::uint16_t>(~input.frets | previous_.frets);
        for(unsigned lane=0;lane<(chart_.kind==ChartKind::SixFret?6u:5u);++lane) {
            const unsigned bit=1u<<lane;
            if(input.frets&bit) unusedRelease_[lane].reset();
            else if(previous_.frets&bit) {
                if(activeSustainFrets_&bit) unusedRelease_[lane].reset();
                else unusedRelease_[lane]=std::nextafter(time+0.007,INFINITY);
            }
        }
        // Guitar input 0x180FCFEB0 arms the same timer for presses AND releases.
        // 0x180FCAA30 advances it only for timed judging modifiers (0x3000).
        // Normal play leaves it armed until a legato hit/miss consumes it;
        // the ctor's one-second value is not a normal-mode expiration.
        if(fretChanged) fretValidUntil_=precisionMode_?time+precisionInput:std::numeric_limits<Seconds>::infinity();
        previous_ = input;
        std::optional<HitResult> hit;
        const bool buffered=strumUntil_.has_value();
        const auto firstPending=evaluator_.nextPendingNote();
        if (strummed || buffered || fretValidUntil_ || autoStrum_) {
            // New physical inputs select the nearest matching in-window head.
            // Otherwise a skipped repeated fret can consume every later strum
            // one note behind. Ties retain the earlier head. Buffered/held
            // input and Precision retain chronological PC dispatch.
            const bool physicalEdge=strummed && physicalStrum && !precisionMode_;
            std::optional<size_t> preferred;
            if(physicalEdge || (fretChanged && !precisionMode_ && !allStrums_)) {
                double distance=std::numeric_limits<double>::infinity();
                for(const auto index:eligible) {
                    const auto& candidate=chart_.notes[index];
                    if(!physicalEdge && !candidate.tap)continue;
                    if(!matchesPcInput(candidate,input.frets))continue;
                    const double error=std::abs(candidate.time-time);
                    if(error<distance){preferred=index;distance=error;}
                }
            }
            // Exactly one head/chord per input edge; skipped heads settle first.
            for (const auto index : eligible) {
                const Note& note = chart_.notes[index];
                if(preferred && (physicalEdge || note.tap) && index!=*preferred)continue;
                const bool fresh=fretValidUntil_ && time<*fretValidUntil_;
                // 0x180FCD9E0: Tap is independent of streak. HOPO requires a
                // streak after a break; before the first break it may start a run.
                const bool first=eligibleFront_ && *eligibleFront_==index;
                // 0x180FCD9E0 restricts a buffered HOPO/Tap to candidate zero
                // while a recovery streak exists. It cannot skip a still-live
                // earlier head merely because the held frets match the next one.
                // Encore: a NEW physical strum may recover onto a later
                // matching head. Keep PC candidate priority for held/buffered
                // legato input; otherwise an unplayed overlapping HOPO can
                // swallow subsequent intentional strums in a packed run.
                // A new tap press OR pull-off can recover without a strum.
                // Holding a matching fret is insufficient: widening normal
                // windows must not make an old overlapping head swallow every
                // later tap, or let one input clear an entire run. Precision
                // retains its PC candidate/timer rules.
                const bool tapRecovery=note.tap && fretChanged && !precisionMode_ && !allStrums_;
                if(!first && streak_>0 && (note.hopo || note.tap) &&
                   !(strummed && physicalStrum) && !tapRecovery)continue;
                const bool legato=fresh && !allStrums_ &&
                    ((note.tap && (first || tapRecovery || (chainBroken_ && streak_==0))) ||
                     (note.hopo && first && (!chainBroken_ || streak_>0)));
                if(!strummed && !buffered && !(autoStrum_ || (gamepadMode_?fretPressed:legato))) continue;
                hit = evaluator_.tryHit(index, time, windowFor_, [&](const Note& candidate) {
                    return matchesPcInput(candidate,input.frets);
                });
                if (hit) {
                    // Guitar hit 0x180FCC920 misses unplayed preceding chords.
                    for(std::size_t before=firstPending.value_or(index);before<index;++before)
                        if(evaluator_.state(before)==NoteState::Pending)
                            events_.push_back({GuitarEventKind::Miss,before,time});
                    const auto skipped=evaluator_.missNotesBefore(index);
                    misses_+=skipped;
                    if(skipped) {fretValidUntil_.reset();breakChain();}
                    if(legato && (noFretGhosting_ || precisionMode_) && ghostCount_>(noFretGhosting_?0u:2u)) {
                        events_.push_back({GuitarEventKind::Ghost,index,time});breakChain();
                    }
                    ++hits_;++streak_;
                    const auto bonus=fretBonus_.award(!chainBroken_ || streak_>=30,note.hopo || note.tap,
                        note.disjoint?std::max(1,std::popcount(note.frets)):1);
                    events_.push_back({GuitarEventKind::Hit,index,time,bonus});
                    ghostCount_=0;usedFrets_|=input.frets & note.frets;
                    strumUntil_.reset();
                    // PC strum hits do not consume the separate fret-change
                    // timer. Legato dispatch consumes it when it performs a hit.
                    if(legato || (!strummed && !buffered))fretValidUntil_.reset();
                    // Default HOPO re-strum protection: double 0x18307A358.
                    if(note.hopo || note.tap) {
                        const auto duration=(strummed || buffered)?(precisionMode_?precisionInput:0.048):
                            (precisionMode_?note.time+windowFor_(index,note).late-time:0.096);
                        hopoStrumUntil_=time+duration;
                    }
                    else hopoStrumUntil_.reset();
                    break;
                }
            }
            if(!hit && strummed) {
                if(hopoStrumUntil_ && time<*hopoStrumUntil_) {
                    if(precisionMode_) hopoStrumUntil_.reset();
                    strumUntil_.reset();
                } else {
                    if(buffered) {recordOverstrum(time);strumUntil_.reset();}
                    // 0x180FC9A80: buffer a nearby strum while frets settle,
                    // bounded by 84ms and the next note's late window.
                    const auto next=evaluator_.nextPendingNote();
                    if(next && chart_.notes[*next].time-time-windowFor_(*next,chart_.notes[*next]).early<=(precisionMode_?precisionInput:0.048) &&
                       chart_.notes[*next].time+windowFor_(*next,chart_.notes[*next]).late>time)
                        strumUntil_=std::min(time+(precisionMode_?0.066:0.084),chart_.notes[*next].time+windowFor_(*next,chart_.notes[*next]).late);
                    else {recordOverstrum(time);}
                }
            }
        }
        std::stable_sort(events_.begin(),events_.end(),[](const auto& a,const auto& b){return a.time<b.time;});
        return hit;
    }
public:
    [[nodiscard]] const std::vector<GuitarEvent>& events() const noexcept {return events_;}
    [[nodiscard]] const InputEvaluator& evaluator() const { return evaluator_; }
    // Bot judges every due chord at its own timestamp, including multiple
    // chords crossed by a slow frame, then uses the normal scoring path.
    std::vector<HitResult> updateBot(Seconds time) {
        events_.clear();
        std::vector<HitResult> result;
        if(!std::isfinite(time)) return result;
        while(botCursor_<chart_.notes.size() && chart_.notes[botCursor_].time<=time) {
            const auto index=botCursor_++;
            const auto hit=evaluator_.tryHit(index,chart_.notes[index].time,windowFor_,
                [](const Note&) {return true;});
            if(hit) {
                result.push_back(*hit);++hits_;++streak_;
                const auto& note=chart_.notes[index];
                const auto bonus=fretBonus_.award(!chainBroken_ || streak_>=30,note.hopo || note.tap,
                    note.disjoint?std::max(1,std::popcount(note.frets)):1);
                events_.push_back({GuitarEventKind::Hit,index,note.time,bonus});
            }
        }
        return result;
    }
    [[nodiscard]] std::size_t hits() const { return hits_; }
    [[nodiscard]] std::size_t misses() const { return misses_; }
    [[nodiscard]] std::size_t overstrums() const { return overstrums_; }
    [[nodiscard]] std::size_t streakBreaks() const {return streakBreaks_;}
private:
    void recordOverstrum(Seconds time) {
        if((hits_==0 && misses_==0) || (!evaluator_.nextPendingNote() && activeSustainFrets_==0)) return; // PC ignores strums before play starts.
        if(autoStrum_)return;
        if(streak_>0 && !allStrums_) {
            if(eligibleFront_ && *eligibleFront_<chart_.notes.size()) {
                const auto& first=chart_.notes[*eligibleFront_];
                // 0x180FC8E20 / 0x1821894D0 protect a live HOPO/Tap
                // candidate while a recovery streak exists. Timer expiration
                // sees the previous step's candidate, before list refresh.
                if(first.hopo || first.tap)return;
            }
        }
        ghostCount_=0;++overstrums_;breakChain();events_.push_back({GuitarEventKind::Overstrum,0,time});
    }
    std::vector<GuitarEvent> events_;
    std::optional<std::size_t> eligibleFront_;
    void breakChain() noexcept {streak_=0;chainBroken_=true;++streakBreaks_;hopoStrumUntil_.reset();}
    bool matchesPcInput(const Note& note,std::uint16_t pressed) const {
        if(chart_.kind==ChartKind::SixFret)return pcSixFretMatch((unsigned(note.frets)<<1)|(note.open||!note.frets?1u:0u),unsigned(pressed)<<1,note.chord);
        // Five-fret wrapper 0x180FCB670 first removes unrelated active
        // extended sustain frets, then also tries recently completed tails.
        if(note.extended && !exactFrets_) {
            const auto filtered=static_cast<std::uint16_t>(pressed & ~(activeSustainFrets_ & ~note.frets));
            if(filtered!=pressed && matchesPcFretMask(note,filtered,false)) return true;
            const auto recent=static_cast<std::uint16_t>(pressed & ~((activeSustainFrets_|completedSustainFrets_) & ~note.frets));
            if(recent!=pressed && matchesPcFretMask(note,recent,false)) return true;
        }
        return matchesPcFretMask(note,pressed,exactFrets_);
    }
    std::uint16_t activeSustainFrets_{},completedSustainFrets_{};
    // PC five-fret engine note matching, GameAssembly VA 0x1820E3750. PC source
    // masks reserve bit 0 for Open and use bits 1..5 for Green..Orange; native
    // chart/input masks use bits 0..4. Default mode permits lower-fret ghosting
    // on singles and HOPO/Tap chords. The drum fallback requests exact masks.
    static bool matchesPcFretMask(const Note& note, std::uint16_t pressedFrets,
                                  bool exactFrets) {
        const unsigned noteMask = (note.open || note.frets == 0 ? 1u : 0u) |
                                  (static_cast<unsigned>(note.frets) << 1u);
        const unsigned pressedMask = static_cast<unsigned>(pressedFrets) << 1u;
        if (pressedMask == noteMask) return true;

        // Match the PC Open-note special case. Open has source bit 0 and a
        // native zero-fret input has source mask 0 after the lane conversion.
        if ((noteMask & 1u) != 0) {
            if ((pressedMask & 1u) == 0)
                return (noteMask & ~1u) == pressedMask;
            if ((noteMask & 0xfffeu) == 0) return pressedMask == 1u;
        }
        if (exactFrets) return false;

        if (!note.chord) {
            return pressedMask > noteMask && (noteMask ^ pressedMask) < noteMask;
        }
        if (!note.hopo && !note.tap) return false;

        const unsigned difference = noteMask ^ pressedMask;
        const unsigned lowestFretBit = noteMask & (0u - noteMask);
        if (lowestFretBit < difference || pressedMask < difference) return false;
        return pressedMask - difference == noteMask;
    }

    // Precision's two neighbor contributions are capped at .0525 seconds.
    // Keep its bound in the exact source arithmetic order, with outward rounding
    // at the search site so boundary-adjacent representable inputs are retained.
    Seconds maximumEarlyWindow() const noexcept {
        return precisionMode_?(((0.0525+0.0525)*1000.0*0.55238+42.0)/1000.0)*0.5:hitWindow_.early;
    }
    const SongChart& chart_;
    InputEvaluator evaluator_;
    GuitarButtons previous_{};
    HitWindow hitWindow_{makeHitWindow(0.070, false)};
    bool gamepadMode_ = false,classicGamepad_=false,autoStrum_=false;
    bool allStrums_ = false;
    bool noFretGhosting_ = false;
    bool precisionMode_ = false;
    bool exactFrets_ = false;
    InputEvaluator::WindowProvider windowFor_ = [this](std::size_t index, const Note&) {
        return noteWindow(index);
    };
    std::size_t hits_ = 0, misses_ = 0, overstrums_ = 0;
    std::size_t botCursor_=0;
    std::size_t streak_{};
    bool chainBroken_{};
    PcFretBonus fretBonus_;
    std::optional<Seconds> lastUpdate_;
    std::size_t streakBreaks_{},ghostCount_{};
    std::uint16_t usedFrets_{};
    std::array<std::optional<Seconds>,6> unusedRelease_{};
    std::optional<Seconds> fretValidUntil_,strumUntil_,hopoStrumUntil_,lastStrum_;
    bool lastStrumUp_{};
    Seconds strumDebounce_{0.005};
};
} // namespace rhythm
