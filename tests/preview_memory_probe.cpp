// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/audio_player.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
int main(int argc,char** argv) {
 if(argc!=3)return 2;rhythm::AudioStemMixer mixer;
 const auto before=std::chrono::steady_clock::now();
 const bool clip=!std::strcmp(argv[1],"clip"),stream=!std::strcmp(argv[1],"stream");
 const bool loaded=stream?mixer.loadGameplayStream(argv[2],nullptr):clip?mixer.loadPreviewClip(argv[2],nullptr,true,0,.5):mixer.loadChartStems(argv[2],nullptr);
 const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
 if(!loaded)return 1;
 std::printf("{\"mode\":\"%s\",\"stems\":%zu,\"duration\":%.6f,\"clip_start\":%.6f,\"clip_length\":%.6f,\"pcm_bytes\":%llu,\"load_ms\":%.3f}\n",
  stream?"stream":clip?"clip":"full",mixer.stemCount(),mixer.durationSeconds(),mixer.previewStartSeconds(),mixer.previewLengthSeconds(),(unsigned long long)mixer.pcmBytes(),ms);
 return 0;
}
