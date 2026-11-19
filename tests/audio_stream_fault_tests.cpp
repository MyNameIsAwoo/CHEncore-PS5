// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/audio_player.hpp"
#include <cassert>
#include <chrono>
#include <thread>
#include <filesystem>
#include <cstdio>
#include <array>
using namespace rhythm;
static void wav(const char* path,uint32_t frames=20*48000,unsigned channels=2){
 FILE* f=std::fopen(path,"wb");assert(f);
 auto u16=[&](uint16_t x){std::fputc(x&255,f);std::fputc(x>>8,f);};auto u32=[&](uint32_t x){u16(x&65535);u16(x>>16);};
 uint32_t bytes=frames*channels*2;std::fwrite("RIFF",1,4,f);u32(bytes+36);std::fwrite("WAVEfmt ",1,8,f);u32(16);u16(1);u16(channels);u32(48000);u32(48000*channels*2);u16(channels*2);u16(16);std::fwrite("data",1,4,f);u32(bytes);
 for(unsigned i=0;i<bytes/2;++i)u16(100);assert(!std::fclose(f));
}
int main(int argc,char** argv){
 assert(argc==2);std::filesystem::create_directories(argv[1]);const auto dir=std::filesystem::path(argv[1]);const auto song=(dir/"song.wav").string(),chart=(dir/"notes.chart").string();wav(song.c_str());
 // A shorter stem must clamp interpolation at its own EOF rather than
 // fade into the longer backing stream's zero-padded interval.
 const auto guitar=(dir/"guitar.wav").string();wav(guitar.c_str(),481);
 AudioStemMixer stream,reference;assert(stream.loadGameplayStream(chart.c_str(),"song.wav"));assert(reference.loadChartStems(chart.c_str(),"song.wav"));
 stream.setPlaybackRate(.75f);reference.setPlaybackRate(.75f);assert(stream.play() && reference.play());
 std::array<int16_t,512> actual{},expected{};
 for(int i=0;i<8;++i){AudioStemMixer::audioFill(actual.data(),256,&stream);AudioStemMixer::audioFill(expected.data(),256,&reference);assert(actual==expected);}
 stream.clear();reference.clear();std::filesystem::remove(guitar);
 assert(stream.loadGameplayStream(chart.c_str(),"song.wav"));assert(stream.play());
 std::filesystem::resize_file(song,44);
 std::array<int16_t,512> pcm{};const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!stream.streamFailed() && std::chrono::steady_clock::now()<until){AudioStemMixer::audioFill(pcm.data(),256,&stream);std::this_thread::sleep_for(std::chrono::microseconds(100));}
 assert(stream.streamFailed() && !stream.playing());const auto position=stream.positionSeconds();AudioStemMixer::audioFill(pcm.data(),256,&stream);for(auto x:pcm)assert(x==0);assert(position==stream.positionSeconds());stream.clear();
 wav(song.c_str());std::atomic<bool> cancel{false};bool loaded=false;
 std::thread loader([&]{loaded=stream.loadGameplayStream(chart.c_str(),"song.wav",&cancel);});cancel=true;loader.join();assert(!loaded);stream.clear();
 for(int i=0;i<20;++i){assert(stream.loadGameplayStream(chart.c_str(),"song.wav"));stream.seekSeconds(i*.1);stream.play();stream.clear();}
 const auto mono=(dir/"mono.wav").string(),stereo=(dir/"stereo.wav").string(),replacement=(dir/"replacement.wav").string();
 wav(mono.c_str(),48000,1);wav(stereo.c_str(),48000,2);
 std::atomic<bool> replacing{true};
 std::thread writer([&]{unsigned index=0;while(replacing){std::filesystem::copy_file((index++&1)?mono:stereo,replacement,std::filesystem::copy_options::overwrite_existing);std::filesystem::rename(replacement,song);}});
 for(int i=0;i<100;++i){
  if(stream.loadGameplayStream(chart.c_str(),"song.wav")){assert(stream.play());AudioStemMixer::audioFill(pcm.data(),256,&stream);for(auto x:pcm)assert(x==100);}
  stream.clear();
 }
 replacing=false;writer.join();assert(stream.loadGameplayStream(chart.c_str(),"song.wav"));stream.clear();
 for(const auto& file:{song,mono,stereo})std::filesystem::remove(file);
 puts("PASS streamed read failure, stopped cursor, cooperative cancellation, repeated pending-seek teardown and 100 mono/stereo source replacements");
}
