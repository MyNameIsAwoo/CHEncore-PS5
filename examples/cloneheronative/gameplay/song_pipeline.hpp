#pragma once
// Adapted from the top-level CloneHeroPS5 song_pipeline files supplied by HOWL.
// PS5 adapter: no exceptions, platform file/audio backends remain separate.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <vector>
namespace rhythm {
using Seconds = double;
struct Note {
    Seconds time = 0, duration = 0;
    std::uint16_t frets = 0;
    // Event properties preserved while the fret mask is transformed.
    bool tap = false, hopo = false, chord = false;
    // PC uses a separate low bit for Open, including mixed Open/fret events.
    bool open = false;
    bool extended = false;
    bool disjoint = false;
    // PC drum runtime flags; guitar paths leave this zero. Drum events use
    // frets as the canonical kick/pad/cymbal mask, with one gem per entry.
    std::uint32_t drumFlags = 0;
};
enum class ChartKind {FiveFret,Drums,SixFret};
struct DrumFill {std::int64_t startTick{};std::size_t activationNote{};};
struct SongChart { std::vector<Note> notes; ChartKind kind=ChartKind::FiveFret;
    int resolution{192};std::vector<DrumFill> drumFills{}; };
[[nodiscard]] bool validateChart(const SongChart& chart);
struct HitWindow {
    Seconds early = 0.070;
    Seconds late = 0.070;
    [[nodiscard]] bool contains(Seconds noteTime, Seconds inputTime) const;
    // 0x1820F2DD0: the rounded sum can already lie outside the inclusive
    // window; advance it only when the subtraction test still accepts it.
    [[nodiscard]] Seconds lateDeadline(Seconds noteTime) const noexcept {
        auto at=noteTime+late;
        if(noteTime-at>=-late)at=std::nextafter(at,INFINITY);
        return at;
    }
};
[[nodiscard]] HitWindow makeHitWindow(Seconds halfWidth, bool lateOnly);
// Recovered neighbor helper only; the final virtual adjustment is unresolved.
[[nodiscard]] Seconds neighborWindowContribution(
    std::optional<Seconds> earlier, std::optional<Seconds> later,
    Seconds minimum, Seconds maximum);

enum class NoteState { Pending, Hit, Missed };
struct HitResult {
    std::size_t noteIndex;
    Seconds error; // Positive means late.
};
class InputEvaluator {
public:
    using WindowProvider = std::function<HitWindow(std::size_t, const Note&)>;
    using InputMatches = std::function<bool(const Note&)>;
    // Chart must outlive this evaluator and remain unchanged.
    explicit InputEvaluator(const SongChart& chart);
    // Between updates only. Both evaluators use the caller's stable active-chart
    // address; the caller swaps chart contents together with the runtime state.
    void swapRuntimeState(InputEvaluator& other) noexcept {
        states_.swap(other.states_);std::swap(firstPending_,other.firstPending_);
    }
    void reset();
    void ignoreNotesBefore(Seconds startTime);
    [[nodiscard]] std::vector<std::size_t> eligibleNotes(
        Seconds inputTime, const WindowProvider& windowFor,
        Seconds maximumEarly = std::numeric_limits<Seconds>::infinity()) const;
    [[nodiscard]] std::optional<HitResult> tryHit(
        std::size_t index, Seconds inputTime,
        const WindowProvider& windowFor, const InputMatches& matches);
    std::vector<std::size_t> expireNotes(
        Seconds currentTime, const WindowProvider& windowFor);
    // Engine judgement outside a manual hit window, e.g. PC drum-roll
    // completion at its late deadline. Only pending entries can transition.
    bool judgeAutomatic(std::size_t index,NoteState outcome);
    std::size_t missNotesBefore(std::size_t index);
    [[nodiscard]] std::optional<std::size_t> nextPendingNote() const;
    [[nodiscard]] NoteState state(std::size_t index) const;
private:
    const SongChart& chart_;
    std::vector<NoteState> states_;
    std::size_t firstPending_{};
    void advancePending() noexcept;
};

struct AudioPositionSource {
    // Backend converts byte/sample position to seconds; nullopt on failure.
    std::function<std::optional<Seconds>()> playbackSeconds;
    Seconds playbackOffset = 0.0;
    [[nodiscard]] std::optional<Seconds> read() const;
};
struct ClockConfig {
    double scale = 1.0;
    double rate = 1.0;
    Seconds audioReferenceOffset = 0.0;
    Seconds gameplayOffset = 0.0;
    Seconds renderOffset = 0.0;
    // Policy value, not a verified original constant.
    Seconds correctionThreshold = 0.001;
    Seconds referenceStart = 0.0;
    Seconds referenceEnd = 1.0e30;
};
class AudioClock {
public:
    explicit AudioClock(ClockConfig config);
    [[nodiscard]] bool valid() const { return valid_; }
    void reset(Seconds timelineTime);
    void update(Seconds deltaTime, std::optional<Seconds> playbackSeconds,
                bool advancing, bool correctionEnabled);
    [[nodiscard]] Seconds gameplayTime() const;
    [[nodiscard]] Seconds renderTime() const;
private:
    ClockConfig config_;
    bool valid_ = false;
    Seconds timelineTime_ = 0.0;
    Seconds pendingCorrection_ = 0.0;
    Seconds appliedCorrection_ = 0.0;
    bool correcting_ = false;
};

struct RenderNote {
    std::size_t noteIndex;
    Seconds timeUntilHit;
    Seconds duration;
    std::uint16_t frets;
};
// Visibility policy is an integration choice, not recovered game behavior.
void submitVisibleNotes(const SongChart& chart, Seconds renderTime,
                        Seconds lookAhead, Seconds trailingTime,
                        const std::function<void(const RenderNote&)>& submit);
} // namespace rhythm
