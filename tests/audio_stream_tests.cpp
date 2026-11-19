// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/audio_player.hpp"
#include <cassert>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <cstdio>
#include <cmath>
using namespace rhythm;
static void consume(AudioStemMixer& stream,AudioStemMixer& full,int blocks) {
 std::array<int16_t,512> a{},b{};
 for(int i=0;i<blocks;){
  const auto before=stream.positionSeconds();AudioStemMixer::audioFill(a.data(),256,&stream);
  if(stream.positionSeconds()==before && stream.playing()){std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
  AudioStemMixer::audioFill(b.data(),256,&full);
  int max=0;double rms=0;for(size_t n=0;n<a.size();++n){const int d=int(a[n])-b[n];max=std::max(max,std::abs(d));rms+=double(d)*d;}
  if(max>=64 || rms/a.size()>=16)std::printf("mismatch block %d position %.8f max %d rms %.3f\n",i,before,max,std::sqrt(rms/a.size()));
  assert(max<64 && rms/a.size()<16);++i;
 }
 assert(std::abs(stream.positionSeconds()-full.positionSeconds())<1./48000);
}
int main(int argc,char** argv){
 assert(argc>=3);std::setvbuf(stdout,nullptr,_IONBF,0);
 const auto dir=std::filesystem::path(argv[1]);std::filesystem::create_directories(dir);
 for(int source=2;source<argc;++source){
  const auto ext=std::filesystem::path(argv[source]).extension();
  const auto song=dir/("song"+ext.string()),guitar=dir/("guitar"+ext.string());
  std::filesystem::copy_file(argv[source],song,std::filesystem::copy_options::overwrite_existing);
  std::filesystem::copy_file(argv[source],guitar,std::filesystem::copy_options::overwrite_existing);
  const auto chart=(dir/"notes.chart").string();AudioStemMixer full,stream;
  assert(full.loadChartStems(chart.c_str(),song.filename().c_str()));
  assert(stream.loadGameplayStream(chart.c_str(),song.filename().c_str()));
  assert(stream.streaming() && stream.stemCount()==2 && stream.pcmBytes()==2*131072*4);
  assert(stream.durationSeconds()==full.durationSeconds());
  assert(stream.play() && full.play());consume(stream,full,std::min(160,int(full.durationSeconds()*48000/256)));
  stream.pause();const auto held=stream.positionSeconds();std::array<int16_t,512> silence;AudioStemMixer::audioFill(silence.data(),256,&stream);
  assert(stream.positionSeconds()==held);for(auto x:silence)assert(x==0);
  for(double seek:{0.,.125,.5,std::min(3.125,stream.durationSeconds()-.3),stream.durationSeconds()-.3}){
   stream.seekSeconds(seek);full.seekSeconds(seek);stream.setPlaybackRate(.75);full.setPlaybackRate(.75);
   stream.setStemGain(0,.3f);full.setStemGain(0,.3f);stream.setGain(.7f);full.setGain(.7f);
   assert(stream.play() && full.play());consume(stream,full,60);
  }
  // Rapid outstanding seeks: only the newest target may become visible.
  for(int n=0;n<20;++n)stream.seekSeconds(.02*n);
  full.seekSeconds(.38);assert(stream.play() && full.play());consume(stream,full,40);
  stream.setPlaybackRate(1);full.setPlaybackRate(1);
  stream.setPlaybackWindow(.125,.125);full.setPlaybackWindow(.125,.125);
  assert(stream.play() && full.play());consume(stream,full,24);
  assert(!stream.playing() && !full.playing());
  AudioStemMixer moved;moved.takeChartStemsFrom(stream);assert(moved.streaming() && !stream.ready());moved.clear();
  std::atomic<bool> cancel{true};assert(!stream.loadGameplayStream(chart.c_str(),song.filename().c_str(),&cancel));
  std::filesystem::remove(song);std::filesystem::remove(guitar);
  std::printf("PASS streamed %s fixed memory, synchronized gain/rate/seek/pause/move/cancel\n",argv[source]);
 }
}
