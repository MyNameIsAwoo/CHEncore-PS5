#include "gameplay/audio_player.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
int main(int argc,char** argv){
 assert(argc==2);
 rhythm::AudioStemMixer m;assert(m.loadUiSounds("assets/pc_ui_sounds"));
 // Missing optional cue cannot block previews.
 using Cue=rhythm::AudioStemMixer::UiSound;
 m.playUiSound(Cue::MenuIntro);assert(!m.uiSoundPlaying(Cue::MenuIntro));
 assert(m.loadUiSounds(argv[1]));
 m.playUiSound(Cue::MenuIntro);assert(m.uiSoundPlaying(Cue::MenuIntro));
 std::array<int16_t,512> b{};unsigned frames=0;bool audible=false;
 while(m.uiSoundPlaying(Cue::MenuIntro)){
  rhythm::AudioStemMixer::audioFill(b.data(),256,&m);frames+=256;
  for(auto s:b)audible|=s!=0;assert(frames<48000*10);
 }
 assert(audible && frames>=431344 && frames<431344+256);
 m.playUiSound(Cue::MenuIntro);assert(m.uiSoundPlaying(Cue::MenuIntro));
 m.stopUiSound(Cue::MenuIntro);assert(!m.uiSoundPlaying(Cue::MenuIntro));
 m.playUiSound(Cue::Confirm);assert(m.uiSoundPlaying(Cue::Confirm));
 std::puts("PASS nine-second 24-bit cue, exact sample completion, optional-file fallback, cancellation and existing UI cues");
}
