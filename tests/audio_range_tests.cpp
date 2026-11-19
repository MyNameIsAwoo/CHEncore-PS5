// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/audio_player.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
int main(int argc,char** argv) {
 std::setvbuf(stdout,nullptr,_IONBF,0);assert(argc>1);
 for(int a=1;a<argc;++a) {
  rhythm::AudioPlayer full;assert(full.loadAudioFile(argv[a]));
  const auto duration=full.durationSeconds();const auto frames=uint64_t(std::llround(duration*48000));
  assert(std::abs(rhythm::AudioPlayer::probeDuration(argv[a])-duration)<1./48000+1e-9);
  std::vector<int16_t> original(frames*2);assert(full.play());
  rhythm::AudioPlayer::audioFill(original.data(),int(frames),&full);
  for(double start:{0.,.123456,.45001,std::min(3.125,duration-.3),duration-.012345}) {
   rhythm::AudioPlayer clip;const auto length=std::min(.25,duration-start);
   assert(clip.loadAudioRange(argv[a],start,length));
   const auto first=uint64_t(std::llround(start*48000)),count=uint64_t(std::llround(length*48000));
   assert(clip.pcmBytes()==count*4 && clip.durationSeconds()==duration);
   assert(std::abs(clip.positionSeconds()-double(first)/48000)<1e-9);
   std::vector<int16_t> actual(count*2);assert(clip.play());
   rhythm::AudioPlayer::audioFill(actual.data(),int(count),&clip);assert(!clip.playing());
   int maximum=0;double rms=0;
   for(size_t i=0;i<actual.size();++i){const int error=int(actual[i])-original[first*2+i];maximum=std::max(maximum,std::abs(error));rms+=double(error)*error;}
   rms=std::sqrt(rms/actual.size());
   std::printf("range %s %.6f max_error %d rms %.4f\n",argv[a],start,maximum,rms);
   assert(rms<4 && maximum<64);
   clip.seekSeconds(start);assert(clip.play());
   clip.setPlaybackWindow(start,.01);assert(clip.play());
   std::array<int16_t,960> buffer;rhythm::AudioPlayer::audioFill(buffer.data(),480,&clip);
   assert(!clip.playing());assert(std::abs(clip.positionSeconds()-double(first+480)/48000)<1e-9);
   rhythm::AudioPlayer moved;moved.takeDecodedFrom(clip);
   assert(moved.ready() && !clip.ready() && moved.durationSeconds()==duration);
   moved.seekSeconds(-1);assert(std::abs(moved.positionSeconds()-double(first)/48000)<1e-9);
   moved.seekSeconds(1e20);assert(!moved.play());
  }
  rhythm::AudioPlayer tail;assert(tail.loadAudioRange(argv[a],duration-.01,60));
  assert(tail.pcmBytes()==480*4);assert(tail.play());
  std::array<int16_t,960> end;rhythm::AudioPlayer::audioFill(end.data(),480,&tail);
  assert(!tail.playing() && std::abs(tail.positionSeconds()-duration)<1e-9);
  rhythm::AudioPlayer invalid;assert(!invalid.loadAudioRange(argv[a],-.1,.1));
  assert(!invalid.loadAudioRange(argv[a],duration+.1,.1));
  assert(!invalid.loadAudioRange(argv[a],0,0));assert(!invalid.loadAudioRange(argv[a],0,.1,4));
 }
 std::puts("PASS bounded audio ranges, sample positioning, PCM budget, seek/window/move lifecycle and full-file comparison");
}
