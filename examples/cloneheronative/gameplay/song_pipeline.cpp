#include "song_pipeline.hpp"
namespace rhythm {
namespace {
bool validWindow(const HitWindow& w) {
    return std::isfinite(w.early) && std::isfinite(w.late) && w.early >= 0 && w.late >= 0;
}
}
bool validateChart(const SongChart& chart) {
    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        const auto& n = chart.notes[i];
        if (!std::isfinite(n.time) || !std::isfinite(n.duration) || n.duration < 0 ||
            !std::isfinite(n.time + n.duration)) return false;
        if(chart.kind==ChartKind::Drums) {
            if(!n.frets || (n.frets&~255u) || (n.frets&(n.frets-1)) ||
                (i && chart.notes[i-1].time>n.time)) return false;
        } else if((n.frets&~(chart.kind==ChartKind::SixFret?63u:31u)) || (i && chart.notes[i-1].time>=n.time)) return false;
    }
    return true;
}
bool HitWindow::contains(Seconds noteTime, Seconds inputTime) const {
    if (!validWindow(*this) || !std::isfinite(noteTime) || !std::isfinite(inputTime)) return false;
    const Seconds delta = noteTime - inputTime;
    return -late <= delta && delta <= early;
}
HitWindow makeHitWindow(Seconds halfWidth, bool lateOnly) {
    if (!std::isfinite(halfWidth) || halfWidth < 0.0) return {-1, -1};
    const HitWindow result = lateOnly ? HitWindow{0.0, 2.0 * halfWidth}
                                      : HitWindow{halfWidth, halfWidth};
    if (!validWindow(result)) return {-1, -1};
    return result;
}
Seconds neighborWindowContribution(std::optional<Seconds> earlier,
                                  std::optional<Seconds> later,
                                  Seconds minimum, Seconds maximum) {
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum < 0 || maximum < minimum ||
        (earlier && !std::isfinite(*earlier)) || (later && !std::isfinite(*later))) return 0;
    if (!earlier || !later || *earlier == *later) return maximum;
    return std::clamp(0.5 * (*later - *earlier), minimum, maximum);
}

InputEvaluator::InputEvaluator(const SongChart& chart)
    : chart_(chart), states_(chart.notes.size(), NoteState::Pending) {
    if (!validateChart(chart)) states_.clear();
}
void InputEvaluator::reset() {
    states_.assign(validateChart(chart_) ? chart_.notes.size() : 0, NoteState::Pending);
    firstPending_=0;
}
void InputEvaluator::ignoreNotesBefore(Seconds startTime) {
    if (!std::isfinite(startTime)) return;
    for (std::size_t i = 0; i < states_.size() && chart_.notes[i].time < startTime; ++i)
        states_[i] = NoteState::Hit;
    advancePending();
}
std::vector<std::size_t> InputEvaluator::eligibleNotes(
    Seconds inputTime, const WindowProvider& windowFor,Seconds maximumEarly) const {
    std::vector<std::size_t> result;
    if (!std::isfinite(inputTime) || !windowFor) return result;
    // The optional bound is supplied only by engines with a known maximum
    // early window. The generic API retains unbounded/custom windows.
    const Seconds horizon=std::nextafter(inputTime+maximumEarly,INFINITY);
    for (std::size_t i = firstPending_; i < states_.size(); ++i) {
        if(chart_.notes[i].time>horizon)break;
        if (states_[i] == NoteState::Pending &&
            windowFor(i, chart_.notes[i]).contains(chart_.notes[i].time, inputTime))
            result.push_back(i);
    }
    return result;
}
std::optional<HitResult> InputEvaluator::tryHit(
    std::size_t index, Seconds inputTime, const WindowProvider& windowFor,
    const InputMatches& matches) {
    if (!std::isfinite(inputTime) || !windowFor || !matches) return std::nullopt;
    if (index >= states_.size() || states_[index] != NoteState::Pending)
        return std::nullopt;
    const auto& note = chart_.notes[index];
    if (!windowFor(index, note).contains(note.time, inputTime) || !matches(note))
        return std::nullopt;
    states_[index] = NoteState::Hit;
    advancePending();
    return HitResult{index, inputTime - note.time};
}
std::vector<std::size_t> InputEvaluator::expireNotes(
    Seconds currentTime, const WindowProvider& windowFor) {
    std::vector<std::size_t> missed;
    if (!std::isfinite(currentTime) || !windowFor) return missed;
    for (std::size_t i = firstPending_; i < states_.size(); ++i) {
        // A valid window has a nonnegative late side. Future heads cannot
        // expire, irrespective of the custom per-note window provider.
        if(chart_.notes[i].time>=currentTime)break;
        if (states_[i] != NoteState::Pending) continue;
        const auto window = windowFor(i, chart_.notes[i]);
        if (!validWindow(window)) continue;
        if (chart_.notes[i].time-currentTime < -window.late) {
            states_[i] = NoteState::Missed;
            missed.push_back(i);
        }
    }
    advancePending();
    return missed;
}
NoteState InputEvaluator::state(std::size_t index) const {
    return index < states_.size() ? states_[index] : NoteState::Missed;
}
bool InputEvaluator::judgeAutomatic(std::size_t index,NoteState outcome) {
    if(index>=states_.size() || states_[index]!=NoteState::Pending || outcome==NoteState::Pending) return false;
    states_[index]=outcome;advancePending();return true;
}
std::size_t InputEvaluator::missNotesBefore(std::size_t index) {
    std::size_t missed=0;
    for(std::size_t i=firstPending_;i<std::min(index,states_.size());++i)
        if(states_[i]==NoteState::Pending) {states_[i]=NoteState::Missed;++missed;}
    advancePending();
    return missed;
}
void InputEvaluator::advancePending() noexcept {
    while(firstPending_<states_.size() && states_[firstPending_]!=NoteState::Pending)++firstPending_;
}
std::optional<std::size_t> InputEvaluator::nextPendingNote() const {
    return firstPending_<states_.size()?std::optional<std::size_t>{firstPending_}:std::nullopt;
}

std::optional<Seconds> AudioPositionSource::read() const {
    if (!playbackSeconds || !std::isfinite(playbackOffset)) return std::nullopt;
    const auto value = playbackSeconds();
    if (!value || !std::isfinite(*value)) return std::nullopt;
    const Seconds adjusted = *value + playbackOffset;
    return std::isfinite(adjusted) ? std::optional<Seconds>{adjusted} : std::nullopt;
}
AudioClock::AudioClock(ClockConfig config) : config_(config) {
    valid_ = true;
    for (double value : {config.scale, config.rate, config.audioReferenceOffset,
                         config.gameplayOffset, config.renderOffset,
                         config.correctionThreshold, config.referenceStart,
                         config.referenceEnd}) valid_ &= std::isfinite(value);
    valid_ &= config.scale > 0 && config.rate >= 0 && config.correctionThreshold >= 0 &&
              config.referenceEnd >= config.referenceStart;
}
void AudioClock::reset(Seconds timelineTime) {
    if (!valid_ || !std::isfinite(timelineTime)) return;
    timelineTime_ = timelineTime;
    pendingCorrection_ = appliedCorrection_ = 0.0;
    correcting_ = false;
}
void AudioClock::update(Seconds deltaTime, std::optional<Seconds> playbackSeconds,
                        bool advancing, bool correctionEnabled) {
    if (!valid_ || !std::isfinite(deltaTime) || deltaTime < 0) return;
    if (!advancing) return;
    timelineTime_ += deltaTime * config_.rate / config_.scale;
    if (!correctionEnabled || !playbackSeconds || !std::isfinite(*playbackSeconds))
        return;
    const Seconds reference = *playbackSeconds / config_.scale +
                              config_.audioReferenceOffset;
    if (!std::isfinite(reference) || reference <= config_.referenceStart ||
        reference >= config_.referenceEnd) return;
    if (!correcting_) {
        const Seconds drift = reference - timelineTime_;
        if (std::abs(drift) <= config_.correctionThreshold) return;
        pendingCorrection_ = drift;
        appliedCorrection_ = 0.0;
        correcting_ = true;
    }
    // Recovered correction shape; intentionally does not clamp overshoot.
    const Seconds step = deltaTime * pendingCorrection_;
    timelineTime_ += step;
    appliedCorrection_ += step;
    if (std::abs(appliedCorrection_) >= std::abs(pendingCorrection_)) {
        correcting_ = false;
        pendingCorrection_ = appliedCorrection_ = 0.0;
    }
}
Seconds AudioClock::gameplayTime() const {
    return timelineTime_ - config_.gameplayOffset;
}
Seconds AudioClock::renderTime() const {
    return timelineTime_ + config_.renderOffset;
}

void submitVisibleNotes(const SongChart& chart, Seconds renderTime,
                        Seconds lookAhead, Seconds trailingTime,
                        const std::function<void(const RenderNote&)>& submit) {
    if (!std::isfinite(renderTime) || !std::isfinite(lookAhead) || !std::isfinite(trailingTime) ||
        lookAhead < 0 || trailingTime < 0 || !submit) return;
    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        const auto& note = chart.notes[i];
        const Seconds relative = note.time - renderTime;
        if (relative > lookAhead) break;
        if (relative + note.duration < -trailingTime) continue;
        submit(RenderNote{i, relative, note.duration, note.frets});
    }
}
} // namespace rhythm
