// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/guitar_session.hpp"
#include "gameplay/drum_session.hpp"
#include <array>
#include <cassert>
#include <memory>
#include <cstdio>
struct Parked {
    rhythm::SongChart chart;
    rhythm::GuitarSession guitar;
    rhythm::DrumSession drums;
    explicit Parked(const rhythm::SongChart& shared):guitar(shared),drums(shared){}
};
int main() {
    using namespace rhythm;
    SongChart shared;
    GuitarSession activeGuitar(shared);DrumSession activeDrums(shared);
    std::array<SongChart,4> isolatedCharts;
    std::array<std::unique_ptr<Parked>,4> parked;
    std::array<std::unique_ptr<GuitarSession>,4> guitars;
    std::array<std::unique_ptr<DrumSession>,4> drums;
    for(unsigned player=0;player<4;++player) {
        auto& chart=isolatedCharts[player];chart.kind=player<2?ChartKind::FiveFret:ChartKind::Drums;
        for(unsigned i=0;i<7+player;++i) {
            Note note{1+i*.5+player*.01,0,uint16_t(1u<<(i%5))};
            if(player==3 && i%3==0){note.frets=32;note.drumFlags=pc_drums::cymbal;}
            chart.notes.push_back(note);
        }
        guitars[player]=std::make_unique<GuitarSession>(chart);
        drums[player]=std::make_unique<DrumSession>(chart);
        parked[player]=std::make_unique<Parked>(shared);parked[player]->chart=chart;
        std::swap(shared,parked[player]->chart);
        activeGuitar.swapRuntimeState(parked[player]->guitar);
        activeDrums.swapRuntimeState(parked[player]->drums);
        activeGuitar.reset();activeDrums.reset();
        activeGuitar.setPrecisionMode(player==1);guitars[player]->setPrecisionMode(player==1);
        activeDrums.setOptions(player==3,false,false,false,100);
        drums[player]->setOptions(player==3,false,false,false,100);
        activeGuitar.swapRuntimeState(parked[player]->guitar);
        activeDrums.swapRuntimeState(parked[player]->drums);
        std::swap(shared,parked[player]->chart);
    }
    unsigned comparisons=0;
    for(unsigned frame=0;frame<1000;++frame)for(unsigned player=0;player<4;++player) {
        const double time=frame/120.;
        std::swap(shared,parked[player]->chart);
        activeGuitar.swapRuntimeState(parked[player]->guitar);
        activeDrums.swapRuntimeState(parked[player]->drums);
        if(player<2) {
            GuitarButtons input;
            for(unsigned i=0;i<shared.notes.size();++i)
                if(frame==unsigned(std::lround(shared.notes[i].time*120)) && i%3!=player) {
                    input.frets=shared.notes[i].frets;input.strumUp=i%2;input.strumDown=!(i%2);
                }
            activeGuitar.update(input,time);guitars[player]->update(input,time);
            assert(activeGuitar.hits()==guitars[player]->hits());
            assert(activeGuitar.misses()==guitars[player]->misses());
            assert(activeGuitar.overstrums()==guitars[player]->overstrums());
            assert(activeGuitar.ghostCount()==guitars[player]->ghostCount());
            assert(activeGuitar.events().size()==guitars[player]->events().size());
            for(unsigned i=0;i<shared.notes.size();++i)assert(activeGuitar.evaluator().state(i)==guitars[player]->evaluator().state(i));
        } else {
            DrumInput input;
            for(unsigned i=0;i<shared.notes.size();++i)
                if(frame==unsigned(std::lround(shared.notes[i].time*120)) && i%3!=player-2)input.strikes|=shared.notes[i].frets;
            activeDrums.update(input,time);drums[player]->update(input,time);
            assert(activeDrums.hits()==drums[player]->hits());
            assert(activeDrums.misses()==drums[player]->misses());
            assert(activeDrums.events().size()==drums[player]->events().size());
            for(unsigned i=0;i<shared.notes.size();++i)assert(activeDrums.evaluator().state(i)==drums[player]->evaluator().state(i));
        }
        activeGuitar.swapRuntimeState(parked[player]->guitar);
        activeDrums.swapRuntimeState(parked[player]->drums);
        std::swap(shared,parked[player]->chart);++comparisons;
    }
    assert(guitars[0]->hits()>0 && guitars[1]->hits()>0 && drums[2]->hits()>0 && drums[3]->hits()>0);
    std::printf("PASS %u interleaved four-player guitar/drum context steps match isolated engines\n",comparisons);
}
