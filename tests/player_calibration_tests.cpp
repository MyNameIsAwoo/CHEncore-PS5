// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/player_calibration.hpp"
#include "gameplay/profile_menu.hpp"
#include "gameplay/midi_setup.hpp"
#include "gameplay/song_pipeline.hpp"
#include "gameplay/drum_session.hpp"
#include "gameplay/guitar_session.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unistd.h>
#define JSON_NOEXCEPTION 1
#include "json.hpp"
using namespace rhythm;
static void timing(unsigned fps){
    std::array<TimingCalibration,4> players;
    const double delays[]{.075,-.032,.112,.005};
    for(auto& player:players)player.begin(0);
    std::array<unsigned,4> next{};
    for(unsigned frame=0;frame<fps*20;++frame){const double now=double(frame)/fps;
        for(unsigned seat=0;seat<4;++seat){
            while(next[seat]<16){
                const double at=players[seat].firstBeat+next[seat]*CalibrationBeatSeconds+delays[seat];
                if(at>now)break;
                assert(players[seat].sample(at));
                assert(!players[seat].sample(at+.002)); // MIDI bounce/chords cannot count twice for a cue.
                ++next[seat];
            }
            players[seat].update(now);
        }
    }
    for(unsigned seat=0;seat<4;++seat){assert(players[seat].valid && players[seat].inliers==16);assert(std::abs(players[seat].offsetMs-delays[seat]*1000)<.0001);}
}
// Advance empty frames on the same calibrated timeline as timestamped hits.
// A slow device must not lose its note to an unshifted automatic miss deadline.
static void judgement(unsigned fps,float offsetMs){
    SongChart guitarChart;guitarChart.notes={{1,0,1},{2,0,2}};
    SongChart drumChart=guitarChart;drumChart.kind=ChartKind::Drums;
    GuitarSession guitar(guitarChart);DrumSession drums(drumChart);
    unsigned next=0;
    for(unsigned frame=0;frame<fps*4;++frame){
        const double shared=double(frame)/fps;
        while(next<2 && 1.+next+offsetMs*.001<=shared){
            const double event=1.+next+offsetMs*.001;
            const double at=playerJudgementTime(event,offsetMs);
            guitar.update({uint16_t(1u<<next),true,false},at);
            drums.update({uint16_t(1u<<next)},at);
            guitar.update({},at+.001);
            ++next;
        }
        const double end=playerJudgementTime(shared,offsetMs);
        guitar.update({},end);drums.update({},end);
    }
    assert(guitar.hits()==2 && guitar.misses()==0);
    assert(drums.hits()==2 && drums.misses()==0);
}
int main(){
    timing(30);timing(60);timing(120);
    for(unsigned fps:{30u,60u,120u})for(float offset:{-240.f,0.f,240.f})judgement(fps,offset);
    TimingCalibration outliers;outliers.begin(0);
    for(unsigned i=0;i<16;++i)assert(outliers.sample(outliers.firstBeat+i*CalibrationBeatSeconds+(i==5?.22:.04)));
    assert(outliers.valid && outliers.inliers==15 && std::abs(outliers.offsetMs-40)<.001);
    TimingCalibration inconsistent;inconsistent.begin(0);
    for(unsigned i=0;i<16;++i)assert(inconsistent.sample(inconsistent.firstBeat+i*CalibrationBeatSeconds+(i%2?.15:-.15)));
    assert(!inconsistent.valid);
    TimingCalibration empty;empty.begin(0);empty.update(40);assert(!empty.valid && empty.state==TimingCalibration::State::Review);
    assert(std::abs(playerJudgementTime(1.1,100)-1)<1e-10);
    assert(std::abs(playerJudgementTime(1.1,100,1.5)-.95)<1e-10);
    assert(playerJudgementTime(1,std::numeric_limits<float>::quiet_NaN())==1);
    // Audio reference is on the sample clock, including the queued grain; event
    // interpolation preserves USB/MIDI event age and detects audio restarts.
    CalibrationTone tone;int16_t pcm[512]{};double t{};uint64_t epoch{};
    assert(!tone.timeAt(1000000,t,epoch));tone.enable(true);
    tone.fill(pcm,256,1000000);assert(tone.timeAt(1000000,t,epoch));assert(std::abs(t-256./48000)<1e-9);
    const auto firstEpoch=epoch;assert(tone.timeAt(1003000,t,epoch) && std::abs(t-(256./48000+.003))<1e-9);
    assert(std::any_of(std::begin(pcm),std::end(pcm),[](auto x){return x!=0;}));
    tone.fill(pcm,256,1200000);assert(tone.timeAt(1200000,t,epoch) && epoch!=firstEpoch);
    tone.enable(false);assert(!tone.timeAt(1200000,t,epoch));
    // Profiles retain separate values for each instrument and each player.
    const auto root=std::filesystem::temp_directory_path()/("encore-calibration-"+std::to_string(getpid()));std::filesystem::create_directories(root);
    const auto path=root/"players.json";std::vector<MenuProfile> profiles;
    for(unsigned seat=0;seat<4;++seat){MenuProfile p{"P"+std::to_string(seat+1)};for(unsigned type=0;type<5;++type)p.inputOffsetsMs[type]=float(seat*20+type*3);profiles.push_back(p);}
    assert(savePlayerProfiles(path.c_str(),profiles));std::vector<MenuProfile> restored;assert(loadPlayerProfiles(path.c_str(),restored));
    for(unsigned seat=0;seat<4;++seat)assert(restored[seat].inputOffsetsMs==profiles[seat].inputOffsetsMs);
    nlohmann::json j;{std::ifstream f(path);f>>j;}
    for(auto& p:j["players"])p.erase("input_offsets_ms");{std::ofstream f(path);f<<j;}
    assert(loadPlayerProfiles(path.c_str(),restored));for(auto& p:restored)for(auto offset:p.inputOffsetsMs)assert(offset==0);
    j["players"][0]["input_offsets_ms"]={0,0,9999,0,0};{std::ofstream f(path);f<<j;}assert(!loadPlayerProfiles(path.c_str(),restored));
    profiles[0].inputOffsetsMs[0]=std::numeric_limits<float>::infinity();assert(!savePlayerProfiles(path.c_str(),profiles));
    ProfileMenu menu;menu.slots[0].profile=-1;menu.slots[0].page=ProfileMenu::Page::Options;
    auto rows=menu.rows(0);menu.slots[0].row=int(std::find(rows.begin(),rows.end(),"Calibrate Controller")-rows.begin());menu.accept(0);
    assert(menu.slots[0].page==ProfileMenu::Page::Calibration);menu.slots[0].row=1;menu.adjust(0,-1);assert(menu.profile(0)->inputOffsetsMs[0]==-1);
    assert(menu.slots[1].guest.inputOffsetsMs[0]==0);menu.back(0);assert(menu.rows(0)[menu.slots[0].row]=="Calibrate Controller");
    // MIDI velocity-only setup preserves two kicks and only asks for mapped
    // pads. A guest's draft never changes the original mapping until committed.
    midi::Profile kit;kit.twoKickPads=true;
    assert(kit.mapping.bind({midi::Action::Kick,0,9,35}));assert(kit.mapping.bind({midi::Action::Kick,0,9,36}));assert(kit.mapping.bind({midi::Action::Red,0,9,38}));
    midi::SetupWizard wizard;wizard.beginVelocity(kit,1000000);assert(wizard.action()==midi::Action::Red);
    assert(wizard.sample({1000000,0,9,38,25}));assert(wizard.sample({1300000,0,9,38,110}));
    assert(wizard.page==midi::SetupWizard::Page::Review && wizard.draft.twoKickPads);
    unsigned kicks=0;for(const auto& b:wizard.draft.mapping.bindings())if(b.action==midi::Action::Kick){++kicks;assert(!b.velocity.calibrated());}
    assert(kicks==2);for(const auto& b:kit.mapping.bindings())assert(!b.velocity.calibrated());
    std::filesystem::remove_all(root);
    puts("PASS player/instrument isolation, timestamp timing at 30/60/120 FPS, duplicate/outlier rejection, audio clock/restarts, profile migration/validation and MIDI velocity without kick remapping");
}
