#pragma once

#include "../gameplay/persistent_storage.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#if !defined(PS5_HOST_REFERENCE)
#include <pthread.h>
#endif

namespace chorus {

struct ChartResult {
    std::string title;
    std::string artist;
    std::string charter;
    std::string md5;
    unsigned encoderVersion{0};
    std::string zipFallbackUrl;
    std::string albumArtMd5;
    std::string albumArtPath;
    bool albumArtRequested{false};
    std::string groupKey;
    // Bridge's notesData.noteCounts: 10 charted instrument types, each using
    // the website's Easy/Medium/Hard/Expert count data.
    uint16_t chartedInstrumentMask{0};
    std::array<uint8_t, 10> instrumentDifficultyMasks{};
    bool hasVocals{false};
    bool hasVideo{false};
};

struct SearchOptions {
    const char* instrument{"guitar"};
    const char* difficulty{"expert"};
    const char* drumType{"fiveLane"};
    const char* sortField{"name"};
    const char* searchField{nullptr};
    bool drumsReviewed{false};
    bool sortDescending{false};
};

class Downloader {
public:
    enum class State : uint8_t { Idle, Searching, Results, Downloading, Complete, Error };
    static constexpr size_t MaxResults = 120;
    static constexpr unsigned MaxPages = 6;

    explicit Downloader(std::string songRoot=CH_SONGS) noexcept : songRoot_(std::move(songRoot)) {}
    ~Downloader() noexcept;
    Downloader(const Downloader&) = delete;
    Downloader& operator=(const Downloader&) = delete;

    bool search(const char* keywords, unsigned page = 1, SearchOptions options = {}) noexcept;
    bool loadNextPage(const char* keywords) noexcept;
    bool download(size_t resultIndex) noexcept;
    bool requestCover(size_t resultIndex) noexcept;
    bool coverPending(size_t resultIndex) const noexcept;
    const char* coverPath(size_t resultIndex) const noexcept;
    void update() noexcept;
    void cancel() noexcept;
    void reset() noexcept;

    State state() const noexcept { return state_; }
    const char* error() const noexcept { return error_.c_str(); }
    size_t resultCount() const noexcept { return resultCount_; }
    bool hasMoreResults() const noexcept { return hasMoreResults_; }
    unsigned page() const noexcept { return page_; }
    const ChartResult* result(size_t i) const noexcept { return i < resultCount_ ? &results_[i] : nullptr; }
    double progress() const noexcept { return progress_.load(std::memory_order_relaxed); }
    const char* installedPath() const noexcept { return installedPath_.c_str(); }

private:
    struct HttpTransfer;
#if !defined(PS5_HOST_REFERENCE)
    static void* transferThread(void* user) noexcept;
#endif
    bool beginTransfer(const char* url, const std::string* postBody, const char* outputPath = nullptr) noexcept;
    void finishTransfer(long httpStatus) noexcept;
    void fail(const char* message) noexcept;
    bool parseSearchResponse() noexcept;

    std::string songRoot_;
    State state_{State::Idle};
    std::array<ChartResult, MaxResults> results_{};
    size_t resultCount_{0};
    size_t selectedResult_{0};
    unsigned page_{0};
    unsigned requestedPage_{0};
    size_t totalResults_{0};
    std::array<std::string, MaxResults> resultGroups_{};
    size_t resultGroupCount_{0};
    bool hasMoreResults_{false};
    SearchOptions searchOptions_{};
    std::string error_;
    std::string installedPath_;
    std::string response_;
    std::string postBody_;
    std::string temporarySng_;
    std::string stageDirectory_;
    std::string finalDirectory_;
    std::atomic<double> progress_{0.0};
    HttpTransfer* transfer_{nullptr};
};

} // namespace chorus
