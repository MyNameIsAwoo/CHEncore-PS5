// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>

namespace rhythm {
inline constexpr double CalibrationBeatSeconds=.75;
inline constexpr float MaxPlayerOffsetMs=500;
inline double playerJudgementTime(double shared,float offsetMs,double rate=1) noexcept {
    if(!std::isfinite(offsetMs))offsetMs=0;
    if(!std::isfinite(rate) || rate<=0)rate=1;
    return shared-std::clamp(double(offsetMs),-double(MaxPlayerOffsetMs),double(MaxPlayerOffsetMs))*.001*rate;
}
// Timestamp-based, one response per beat; never a frame counter or a gameplay
// debounce. Median/MAD reject isolated mis-taps without widening hit windows.
class TimingCalibration {
public:
    static constexpr unsigned Samples=16;
    enum class State {Idle,Collect,Review};
    State state{State::Idle};
    unsigned count{},inliers{};
    double firstBeat{},offsetMs{},spreadMs{};
    bool valid{};
    void begin(double now) noexcept {
        *this={};if(!std::isfinite(now) || now<0)return;
        state=State::Collect;firstBeat=(std::ceil(now/CalibrationBeatSeconds)+4)*CalibrationBeatSeconds;
        used_.fill(false);
    }
    bool sample(double time) noexcept {
        if(state!=State::Collect || !std::isfinite(time))return false;
        const auto index=std::llround((time-firstBeat)/CalibrationBeatSeconds);
        if(index<0 || index>=int(used_.size()) || used_[size_t(index)])return false;
        const double error=time-(firstBeat+index*CalibrationBeatSeconds);
        if(std::abs(error)>.30)return false;
        used_[size_t(index)]=true;values_[count++]=error*1000;
        if(count==Samples)finish();
        return true;
    }
    void update(double now) noexcept {
        if(state==State::Collect && now>firstBeat+used_.size()*CalibrationBeatSeconds)finish();
    }
    void finish() noexcept {
        state=State::Review;valid=false;inliers=0;
        if(count<12)return;
        auto sorted=values_;std::sort(sorted.begin(),sorted.begin()+count);
        const double median=(sorted[(count-1)/2]+sorted[count/2])*.5;
        std::array<double,Samples> deviations{};
        for(unsigned i=0;i<count;++i)deviations[i]=std::abs(values_[i]-median);
        std::sort(deviations.begin(),deviations.begin()+count);
        const double mad=(deviations[(count-1)/2]+deviations[count/2])*.5;
        const double tolerance=std::max(15.0,3*1.4826*mad);
        double sum=0,square=0;
        for(unsigned i=0;i<count;++i)if(std::abs(values_[i]-median)<=tolerance){++inliers;sum+=values_[i];square+=values_[i]*values_[i];}
        if(inliers<12)return;
        offsetMs=sum/inliers;spreadMs=std::sqrt(std::max(0.0,square/inliers-offsetMs*offsetMs));
        valid=std::abs(offsetMs)<=300 && spreadMs<=50;
    }
private:
    std::array<double,Samples> values_{};
    std::array<bool,40> used_{};
};

// A short mathematical reference tone, generated in the existing audio callback
// with no resource files, allocations or locks. Its published sample clock and
// monotonic timestamp put USB/MIDI responses on the audio timeline, not UI FPS.
class CalibrationTone {
public:
    void enable(bool value,float gain=.5f) noexcept {
        gain_.store(std::clamp(gain,0.0f,1.0f),std::memory_order_relaxed);
        if(value!=enabled_.load(std::memory_order_relaxed)){
            request_.fetch_add(1,std::memory_order_acq_rel);enabled_.store(value,std::memory_order_release);
        }
    }
    bool active() const noexcept {return enabled_.load(std::memory_order_acquire);}
    bool timeAt(uint64_t stampUs,double& seconds,uint64_t& epoch) const noexcept {
        if(!active())return false;
        for(unsigned retry=0;retry<4;++retry){
            const auto first=sequence_.load(std::memory_order_acquire);if(first&1)continue;
            const auto stamp=stamp_.load(std::memory_order_relaxed),start=start_.load(std::memory_order_relaxed);
            const auto frames=publishedFrames_.load(std::memory_order_relaxed),generation=publishedRequest_.load(std::memory_order_relaxed);
            const auto e=epoch_.load(std::memory_order_relaxed);
            if(first!=sequence_.load(std::memory_order_acquire))continue;
            if(!stamp || stampUs<start || generation!=request_.load(std::memory_order_acquire))return false;
            const double delta=(double(stampUs)-double(stamp))/1000000.0;
            if(std::abs(delta)>.2)return false;
            seconds=double(frames)/48000.0+delta;epoch=e;return std::isfinite(seconds);
        }
        return false;
    }
    void fill(int16_t* stereo,unsigned frames,uint64_t stampUs) noexcept {
        const auto request=request_.load(std::memory_order_acquire);
        const bool reset=request!=audioRequest_ || !lastStamp_ || stampUs>lastStamp_+100000;
        if(reset){audioRequest_=request;audioFrames_=0;++audioEpoch_;audioStart_=stampUs;}
        const float gain=gain_.load(std::memory_order_relaxed);
        constexpr uint64_t beatFrames=36000,toneFrames=960;
        for(unsigned i=0;i<frames;++i){
            const auto phase=(audioFrames_+i)%beatFrames;
            const double envelope=phase<toneFrames?(1.0-double(phase)/toneFrames)*std::min(1.0,double(phase)/24):0;
            const int16_t sample=phase<toneFrames?int16_t(6000*gain*envelope*std::sin(6.283185307179586*1400*double(phase)/48000.0)):0;
            stereo[i*2]=stereo[i*2+1]=sample;
        }
        audioFrames_+=frames;lastStamp_=stampUs;
        sequence_.fetch_add(1,std::memory_order_acq_rel);
        stamp_.store(stampUs,std::memory_order_relaxed);start_.store(audioStart_,std::memory_order_relaxed);
        publishedFrames_.store(audioFrames_,std::memory_order_relaxed);publishedRequest_.store(request,std::memory_order_relaxed);
        epoch_.store(audioEpoch_,std::memory_order_relaxed);sequence_.fetch_add(1,std::memory_order_release);
    }
private:
    std::atomic<bool> enabled_{false};std::atomic<float> gain_{.5f};
    std::atomic<uint64_t> request_{0},sequence_{0},stamp_{0},start_{0},publishedFrames_{0},publishedRequest_{0},epoch_{0};
    uint64_t audioRequest_{std::numeric_limits<uint64_t>::max()},audioFrames_{},audioEpoch_{},audioStart_{},lastStamp_{};
};
}
