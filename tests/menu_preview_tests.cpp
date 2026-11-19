// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#include "../examples/cloneheronative/gameplay/menu_preview.hpp"
#include "../examples/cloneheronative/gameplay/gameplay_audio.hpp"
#include "../examples/cloneheronative/gameplay/audio_player.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <limits>

static void wav(const std::string& path,int seconds,int16_t amplitude) {
    FILE* f=std::fopen(path.c_str(),"wb"); assert(f);
    const auto u16=[&](uint16_t n){std::fputc(n&255,f);std::fputc(n>>8,f);};
    const auto u32=[&](uint32_t n){u16(n&65535);u16(n>>16);};
    const uint32_t bytes=seconds*48000*4;
    std::fwrite("RIFF",1,4,f);u32(36+bytes);std::fwrite("WAVEfmt ",1,8,f);
    u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);
    std::fwrite("data",1,4,f);u32(bytes);
    for(uint32_t i=0;i<bytes/2;++i)u16(static_cast<uint16_t>(amplitude));
    assert(std::fclose(f)==0);
}
int main(int argc,char** argv) {
    assert(argc==2);
    using namespace rhythm;
    for(double duration:{0.,1.,29.5,30.,44.,45.,50.,59.,60.,61.,67.,90.,900.}) {
        for(double unit:{0.,.001,.5,.999,1.}) {
            const auto window=menuPreviewWindow(duration,false,unit);
            assert(window.start>=0 && window.length>=0 && window.length<=60);
            assert(window.start+window.length<=duration+1e-8);
            assert(window.length==std::min(duration,60.0));
            if(duration-window.length>=7)assert(window.start>=5 && window.start+window.length<=duration-2+1e-8);
            const auto dedicated=menuPreviewWindow(duration,true,unit);
            assert(dedicated.start==0 && dedicated.length==std::min(60.,duration));
        }
    }
    assert(menuPreviewWindow(std::numeric_limits<double>::quiet_NaN(),false,.5).length==0);
    uint32_t rng=0x129a;
    for(int count:{1,2,10,11,12,30}) {
        PreviewHistory history;
        std::vector<std::string> paths,recent;
        for(int i=0;i<count;++i)paths.push_back("chart-"+std::to_string(i));
        for(int j=0;j<10000;++j) {
            rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
            if(j%19==0)std::reverse(paths.begin(),paths.end());
            const int picked=history.choose(paths,rng);
            assert(picked>=0 && picked<count);
            const std::string path=paths[static_cast<size_t>(picked)];
            const size_t buffer=count>10?10:(count>1?1:0);
            for(size_t n=0;n<std::min(buffer,recent.size());++n)assert(path!=recent[n]);
            recent.insert(recent.begin(),path);if(recent.size()>10)recent.resize(10);
            history.remember(path);
        }
    }
    PreviewHistory empty;assert(empty.choose({},0)==-1);
    const std::string folder=argv[1],chart=folder+"/notes.chart";
    wav(folder+"/song.wav",90,100);wav(folder+"/guitar.wav",90,200);wav(folder+"/drums.wav",90,400);
    AudioStemMixer mixer;assert(mixer.loadPreviewStream(chart.c_str(),"song.wav"));
    assert(mixer.stemCount()==3 && !mixer.dedicatedPreview() && mixer.durationSeconds()==90);
    for(size_t stem=0;stem<14;++stem)mixer.setStemGain(stem,missStemGain(0,stem,true,0));
    assert(mixer.play());std::array<int16_t,512> gatePcm{};AudioStemMixer::audioFill(gatePcm.data(),256,&mixer);
    for(auto sample:gatePcm)assert(sample==500); // backing+drums keep playing, guitar muted
    for(size_t stem=0;stem<14;++stem)mixer.setStemGain(stem,1);
    mixer.setPlaybackWindow(7,30);assert(mixer.play());
    std::array<int16_t,512> pcm{};
    for(int block=0;block<5625;++block) {
        AudioStemMixer::audioFill(pcm.data(),256,&mixer);
        for(const auto sample:pcm)assert(sample==700);
    }
    assert(!mixer.playing() && !mixer.play());assert(mixer.positionSeconds()==37);
    AudioStemMixer::audioFill(pcm.data(),256,&mixer);for(auto sample:pcm)assert(sample==0);
    mixer.setPlaybackWindow(7.123456789,30);const double randomStart=mixer.positionSeconds();assert(mixer.play());
    for(int block=0;block<5625;++block)AudioStemMixer::audioFill(pcm.data(),256,&mixer);
    assert(!mixer.playing() && std::abs(mixer.positionSeconds()-randomStart-30)<1e-8);
    mixer.setPlaybackWindow(7.123456789,60);const double longStart=mixer.positionSeconds();assert(mixer.play());
    for(int block=0;block<11250;++block)AudioStemMixer::audioFill(pcm.data(),256,&mixer);
    assert(!mixer.playing() && std::abs(mixer.positionSeconds()-longStart-60)<1e-8);
    mixer.seekSeconds(0);assert(mixer.play());AudioStemMixer::audioFill(pcm.data(),256,&mixer);
    for(auto sample:pcm)assert(sample==700);
    // Menu playback stores only the requested slice, retaining absolute song
    // timestamps and all stems. It must never be reused as complete gameplay PCM.
    AudioStemMixer sliced;assert(sliced.loadPreviewClip(chart.c_str(),"song.wav",true,0,.5));
    assert(sliced.previewClip() && !sliced.dedicatedPreview() && sliced.stemCount()==3);
    assert(sliced.previewLengthSeconds()==60 && sliced.durationSeconds()==90 && sliced.previewFadeOutSeconds()==5 && sliced.previewFadeInSeconds()==1);
    assert(sliced.pcmBytes()==uint64_t(3)*60*48000*4);
    const auto sliceStart=sliced.previewStartSeconds();assert(sliceStart>=5 && sliceStart+60<=88);
    sliced.setPlaybackWindow(sliceStart,60);assert(sliced.play());
    for(int block=0;block<11250;++block){
        AudioStemMixer::audioFill(pcm.data(),256,&sliced);
        for(int frame=0;frame<256;++frame){
            const auto remaining=60*48000-(block*256+frame);
            const auto expected=int(700*std::min({1.0,double(remaining)/(5*48000),double(block*256+frame)/48000}));
            for(int channel=0;channel<2;++channel)assert(std::abs(int(pcm[frame*2+channel])-expected)<=1);
        }
    }
    assert(!sliced.playing() && std::abs(sliced.positionSeconds()-sliceStart-60)<1e-8);
    AudioStemMixer movedSlice;movedSlice.takeChartStemsFrom(sliced);
    assert(movedSlice.previewFadeOutSeconds()==5 && movedSlice.previewFadeInSeconds()==1 && sliced.previewFadeOutSeconds()==0 && sliced.previewFadeInSeconds()==0);
    movedSlice.seekSeconds(sliceStart+59.5);assert(movedSlice.play());
    // Cue is authored test PCM in a WAV container; the reader detects its format.
    wav(folder+"/UIConfirm.opus",1,1000);assert(movedSlice.loadUiSounds(folder.c_str()));
    movedSlice.playUiSound(AudioStemMixer::UiSound::Confirm);
    AudioStemMixer::audioFill(pcm.data(),256,&movedSlice);assert(std::abs(int(pcm[0])-570)<=1);
    AudioStemMixer listSlice;assert(listSlice.loadPreviewClip(chart.c_str(),"song.wav",false,17,.5));
    assert(listSlice.previewClip() && listSlice.previewStartSeconds()==17 && listSlice.previewLengthSeconds()==30 && listSlice.previewFadeOutSeconds()==0 && listSlice.previewFadeInSeconds()==0);
    assert(listSlice.pcmBytes()==uint64_t(3)*30*48000*4);
    wav(folder+"/preview.wav",9,1200);
    assert(mixer.loadPreviewStream(chart.c_str(),"song.wav"));
    assert(mixer.dedicatedPreview() && mixer.stemCount()==1 && mixer.durationSeconds()==9);
    assert(listSlice.loadPreviewClip(chart.c_str(),"song.wav",true,99,.5));
    assert(listSlice.previewClip() && listSlice.dedicatedPreview() && listSlice.previewStartSeconds()==0 && listSlice.previewLengthSeconds()==9 && listSlice.previewFadeOutSeconds()==0 && listSlice.previewFadeInSeconds()==1);
    listSlice.setPlaybackWindow(0,9);assert(listSlice.play());
    AudioStemMixer::audioFill(pcm.data(),256,&listSlice);assert(pcm[0]==0 && pcm[510]>0);
    listSlice.seekSeconds(.5);assert(listSlice.play());AudioStemMixer::audioFill(pcm.data(),256,&listSlice);assert(std::abs(int(pcm[0])-600)<=1);
    listSlice.seekSeconds(1);assert(listSlice.play());AudioStemMixer::audioFill(pcm.data(),256,&listSlice);assert(pcm[0]==1200);
    listSlice.seekSeconds(8.5);assert(listSlice.play());AudioStemMixer::audioFill(pcm.data(),256,&listSlice);for(auto sample:pcm)assert(sample==1200);
    AudioStemMixer moved;moved.takeChartStemsFrom(mixer);
    assert(moved.dedicatedPreview() && !mixer.ready() && !mixer.dedicatedPreview());
    moved.setPlaybackWindow(0,30);assert(moved.play());AudioStemMixer::audioFill(pcm.data(),256,&moved);
    for(auto sample:pcm)assert(sample==1200);
    moved.setGain(0);AudioStemMixer::audioFill(pcm.data(),256,&moved);for(auto sample:pcm)assert(sample==0);
    const double mutedPosition=moved.positionSeconds();assert(mutedPosition>0);
    moved.setGain(1);AudioStemMixer::audioFill(pcm.data(),256,&moved);for(auto sample:pcm)assert(sample==1200);
    assert(moved.positionSeconds()>mutedPosition);
    std::filesystem::rename(folder+"/preview.wav",folder+"/Preview.WAV");
    assert(mixer.loadPreviewStream(chart.c_str(),"song.wav") && mixer.dedicatedPreview());
    std::filesystem::remove(folder+"/Preview.WAV");
    FILE* broken=std::fopen((folder+"/preview.ogg").c_str(),"wb");assert(broken);std::fputs("broken",broken);std::fclose(broken);
    assert(mixer.loadPreviewStream(chart.c_str(),"song.wav") && mixer.stemCount()==3 && !mixer.dedicatedPreview());
    std::filesystem::rename(folder+"/song.wav",folder+"/SONG.WAV");
    assert(mixer.loadChartStems(chart.c_str(),"Song.WAV") && mixer.stemCount()==3 && mixer.previewFadeOutSeconds()==0 && mixer.previewFadeInSeconds()==0);
    const auto shortFolder=folder+"/short";std::filesystem::create_directory(shortFolder);
    wav(shortFolder+"/song.wav",3,1000);AudioStemMixer shortPreview;
    assert(shortPreview.loadPreviewClip((shortFolder+"/notes.chart").c_str(),nullptr,true,0,.5));
    assert(shortPreview.previewFadeOutSeconds()==3);shortPreview.seekSeconds(1.5);assert(shortPreview.play());
    AudioStemMixer::audioFill(pcm.data(),256,&shortPreview);assert(std::abs(int(pcm[0])-500)<=1);
    std::puts("PASS preview full-stem mix, dedicated files, case/corrupt fallback, 60-second windows, one-second widget fade-in, five-second fallback fade-out, list/gameplay exclusions, unfaded UI cues, exact sample boundaries, mute, seek reset and shuffled recent buffer");
}
