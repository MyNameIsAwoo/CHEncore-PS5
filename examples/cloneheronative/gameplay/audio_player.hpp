#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>

namespace rhythm {
class StreamedSong;

// Bounded Opus/Vorbis/MP3/WAV reader for a CH song stem. Decoded PCM is kept in
// title memory so the sceAudioOut callback only mixes/copies; it never opens a
// file or allocates while the hardware audio thread is running.
class AudioPlayer {
public:
    AudioPlayer() = default;
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;
    ~AudioPlayer();

    // The caller must stop audio output before replacing or clearing PCM.
    [[nodiscard]] bool loadAudioFile(const char* path,
                                   uint64_t maxDecodedBytes = uint64_t{48000} * 60 * 15 * 2 * sizeof(int16_t)) noexcept;
    [[nodiscard]] static double probeDuration(const char* path) noexcept;
    [[nodiscard]] bool loadAudioRange(const char* path,double start,double length,
        uint64_t maxDecodedBytes=uint64_t{48000}*60*2*sizeof(int16_t),const std::atomic<bool>* cancel=nullptr) noexcept;
    void clear() noexcept;
    void takeDecodedFrom(AudioPlayer& source) noexcept;
    void rewind() noexcept;
    void seekSeconds(double seconds) noexcept;
    // Caller pauses/stops output before changing the synchronized playback range.
    void setPlaybackWindow(double start,double length) noexcept;
    // Source-frame advance per output frame. Values above 1 speed up playback
    // with linear PCM resampling and change pitch, like conventional practice
    // speed playback.
    void setPlaybackRate(float rate) noexcept;
    [[nodiscard]] bool play() noexcept;
    void pause() noexcept;
    [[nodiscard]] bool ready() const noexcept { return pcm_ != nullptr && frameCount_ != 0; }
    [[nodiscard]] bool playing() const noexcept { return playing_.load(std::memory_order_acquire); }
    [[nodiscard]] double positionSeconds() const noexcept;
    [[nodiscard]] double durationSeconds() const noexcept;
    [[nodiscard]] uint64_t pcmBytes() const noexcept { return frameCount_ * 2 * sizeof(int16_t); }
    void setGain(float gain) noexcept;

    static void audioFill(int16_t* output, int frames, void* user) noexcept;

private:
    void fill(int16_t* output, int frames) noexcept;
    int16_t* pcm_{};               // interleaved 48 kHz stereo
    uint64_t frameCount_{},originFrames_{},totalFrames_{};
    std::atomic<uint64_t> cursor_{};
    std::atomic<uint64_t> playbackEndFrames_{};
    std::atomic<uint64_t> playbackStep_{uint64_t{1} << 32};
    std::atomic<bool> playing_{};
    std::atomic<float> gain_{1.0f};
};

// CH PC's BassAudioManager checks each known stem name against the
// supported container extensions. Gameplay streams through fixed PCM rings;
// previews/cues retain bounded decoded clips. All stems share one playback cursor.
class AudioStemMixer {
public:
    static constexpr size_t StemCount = 14;
    // These names mirror the extracted PC audio assets. Gameplay events select
    // the matching PC cue; BadNote variants are rotated for misses.
    enum class UiSound : size_t {
        Confirm, Cancel, ScrollUp, ScrollDown,
        BadNote1, BadNote2, BadNote3, BadNote4, BadNote5,
        LoseMultiplier, GhostDecrease, NotesRippleUp, StarPowerAwarded,
        StarAvailable, StarDeployed, StarRelease, RewindHighway, MenuIntro,
        Count
    };
    AudioStemMixer() noexcept { stemGains_.fill(1.0f); }
    ~AudioStemMixer();
    AudioStemMixer(const AudioStemMixer&) = delete;
    AudioStemMixer& operator=(const AudioStemMixer&) = delete;

    [[nodiscard]] bool loadChartStems(const char* chartPath, const char* musicStream) noexcept;
    [[nodiscard]] bool loadGameplayStream(const char* chartPath,const char* musicStream,const std::atomic<bool>* cancel=nullptr) noexcept;
    [[nodiscard]] bool streaming() const noexcept {return stream_!=nullptr;}
    [[nodiscard]] bool streamFailed() const noexcept;
    void sourceUnavailable() noexcept;
    [[nodiscard]] bool streamBuffering() const noexcept;
    [[nodiscard]] uint64_t streamUnderruns() const noexcept;
    [[nodiscard]] bool dedicatedPreview() const noexcept { return dedicatedPreview_; }
    [[nodiscard]] bool loadPreviewStream(const char* chartPath, const char* musicStream) noexcept;
    [[nodiscard]] bool loadPreviewClip(const char* chartPath,const char* musicStream,bool mainMenu,
                                       double authoredStart,double randomUnit,const std::atomic<bool>* cancel=nullptr) noexcept;
    [[nodiscard]] bool previewClip() const noexcept {return previewClip_;}
    [[nodiscard]] double previewStartSeconds() const noexcept {return previewStart_;}
    [[nodiscard]] double previewLengthSeconds() const noexcept {return previewLength_;}
    [[nodiscard]] double previewFadeInSeconds() const noexcept {return previewFadeIn_;}
    [[nodiscard]] double previewFadeOutSeconds() const noexcept {return previewFadeOut_;}
    [[nodiscard]] uint64_t pcmBytes() const noexcept;
    void takeChartStemsFrom(AudioStemMixer& source) noexcept;
    [[nodiscard]] bool loadUiSounds(const char* directory,const char* introPath=nullptr) noexcept;
    void playUiSound(UiSound sound) noexcept;
    [[nodiscard]] bool uiSoundPlaying(UiSound sound) const noexcept {
        const size_t i=static_cast<size_t>(sound);
        return i<uiPlayers_.size() && uiLoaded_[i] && uiPlayers_[i].playing();
    }
    void stopUiSound(UiSound sound) noexcept {
        const size_t i=static_cast<size_t>(sound);
        if(i<uiPlayers_.size())uiPlayers_[i].pause();
    }
    void setUiGain(float gain) noexcept;
    void setMissSoundGain(float gain) noexcept;
    void clear() noexcept;
    void seekSeconds(double seconds) noexcept;
    // Caller pauses/stops output before changing the synchronized playback range.
    void setPlaybackWindow(double start,double length) noexcept;
    void setPlaybackRate(float rate) noexcept;
    [[nodiscard]] bool play() noexcept;
    void pause() noexcept;
    [[nodiscard]] bool ready() const noexcept { return loadedCount_ != 0; }
    [[nodiscard]] bool playing() const noexcept;
    [[nodiscard]] size_t stemCount() const noexcept { return loadedCount_; }
    // Diagnostics count non-silent blocks delivered to the real audio callback.
    uint64_t songBlocks() const noexcept { return songBlocks_.load(std::memory_order_relaxed); }
    uint64_t effectBlocks() const noexcept { return effectBlocks_.load(std::memory_order_relaxed); }
    [[nodiscard]] double positionSeconds() const noexcept;
    [[nodiscard]] double durationSeconds() const noexcept;
    void setGain(float gain) noexcept;
    void setStemGain(size_t stem, float gain) noexcept;

    static void audioFill(int16_t* output, int frames, void* user) noexcept;

private:
    struct PreviewRequest {bool menu;double authoredStart,randomUnit;};
    bool loadStems(const char* chartPath,const char* musicStream,const PreviewRequest* preview,bool stream=false,const std::atomic<bool>* cancel=nullptr) noexcept;
    StreamedSong* stream_{};
    std::array<AudioPlayer, StemCount> players_{};
    std::array<AudioPlayer, static_cast<size_t>(UiSound::Count)> uiPlayers_{};
    std::array<bool, static_cast<size_t>(UiSound::Count)> uiLoaded_{};
    std::array<bool, StemCount> loaded_{};
    std::array<int16_t, 256 * 2> scratch_{};
    std::array<int32_t, 256 * 2> accumulator_{};
    std::array<float, StemCount> stemGains_{};
    float masterGain_{1.0f};
    float uiGain_{0.5f}, missSoundGain_{1.0f};
    size_t loadedCount_{};
    bool dedicatedPreview_{},previewClip_{};
    double previewStart_{},previewLength_{},previewFadeOut_{},previewFadeIn_{};
    std::atomic<uint64_t> songBlocks_{},effectBlocks_{};
};

} // namespace rhythm
