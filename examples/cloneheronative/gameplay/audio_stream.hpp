#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
namespace rhythm {
// One producer owns every decoder and file. The callback consumes a common
// ring interval for all stems; no file access, allocation, locks or waits there.
// Replacing/destroying this object requires audio output to be stopped.
class StreamedSong {
public:
 static constexpr size_t Stems=14;
 static StreamedSong* create(const std::array<std::string,Stems>& paths,const std::atomic<bool>* cancel=nullptr) noexcept;
 ~StreamedSong();
 StreamedSong(const StreamedSong&)=delete;
 StreamedSong& operator=(const StreamedSong&)=delete;
 void seek(double seconds) noexcept;
 void window(double start,double length) noexcept;
 bool play() noexcept;
 void pause() noexcept;
 bool playing() const noexcept;
 bool failed() const noexcept;
 void sourceUnavailable() noexcept;
 bool buffering() const noexcept;
 double position() const noexcept;
 double duration() const noexcept;
 uint64_t pcmBytes() const noexcept;
 uint64_t underruns() const noexcept;
 void rate(float value) noexcept;
 void gain(size_t stem,float value) noexcept;
 void mix(int32_t* output,int frames) noexcept;
private:
 struct State;
 explicit StreamedSong(State* state) noexcept:state_(state){}
 State* state_;
};
}
