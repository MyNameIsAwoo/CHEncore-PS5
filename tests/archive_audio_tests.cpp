// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#include "../examples/cloneheronative/gameplay/audio_player.hpp"
#include <cassert>
#include <array>
#include <cmath>
#include <cstdio>
int main(int argc,char** argv){
 assert(argc>=2);
 for(int i=1;i<argc;++i){
  rhythm::AudioPlayer player;assert(player.loadAudioFile(argv[i]));
  assert(player.durationSeconds()>=0.99&&player.durationSeconds()<1.1);
  assert(player.play());std::array<int16_t,8192> pcm{};rhythm::AudioPlayer::audioFill(pcm.data(),4096,&player);
  double energy=0;for(auto s:pcm)energy+=static_cast<double>(s)*s;assert(energy>10000);
  assert(std::abs(player.positionSeconds()-4096.0/48000)<1e-8);
  player.clear();assert(!player.ready());assert(!player.loadAudioFile(argv[i],4));assert(!player.ready());
 }
 std::puts("PASS archive audio decoding, 48-kHz resampling, playback and PCM budgets");
}
