// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace rhythm::controls {
struct RawInputReport {
    uint64_t receivedUs{};
    unsigned length{};
    std::array<uint8_t,64> bytes{};
};
struct RawInputBatch {
    static constexpr unsigned capacity=256;
    std::array<RawInputReport,capacity> reports;
    unsigned count{},dropped{};
};
// One producer and consumer, synchronized by the owning backend. Bounded,
// allocation-free history preserves press/release pulses between display frames.
class RawInputQueue {
public:
    void reset() noexcept {head_=count_=dropped_=0;}
    void push(const RawInputReport& report) noexcept {
        if(count_==RawInputBatch::capacity){head_=(head_+1)%RawInputBatch::capacity;--count_;++dropped_;}
        reports_[(head_+count_)%RawInputBatch::capacity]=report;++count_;
    }
    void drain(RawInputBatch& out) noexcept {
        out.count=count_;out.dropped=dropped_;
        for(unsigned i=0;i<count_;++i)out.reports[i]=reports_[(head_+i)%RawInputBatch::capacity];
        reset();
    }
private:
    std::array<RawInputReport,RawInputBatch::capacity> reports_;
    unsigned head_{},count_{},dropped_{};
};
// Map acquisition time to the current audio-backed clock. Never apply a future
// report early or move judgement time backwards after a pause/seek/correction.
inline double rawInputSongTime(uint64_t receivedUs,uint64_t nowUs,double songTime,
                               double previousTime,double rate) noexcept {
    const double age=nowUs>receivedUs?double(nowUs-receivedUs)/1000000.0:0.0;
    return std::clamp(songTime-age*rate,std::min(previousTime,songTime),songTime);
}
}
