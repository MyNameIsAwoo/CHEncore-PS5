// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#pragma once
#include "pc_drums.hpp"
#include <span>
#include <functional>

namespace rhythm {
// One input mask is a set of fresh strike events, not held controller buttons.
// The adapter owns edge detection and optional hardware velocity translation.
struct DrumInput {uint16_t strikes{};std::array<int,8> velocities{-1,-1,-1,-1,-1,-1,-1,-1};};
enum class DrumEventKind {Hit,Miss,Overhit,Activate};
struct DrumEvent {DrumEventKind kind;size_t noteIndex;Seconds time;unsigned points{};};
class DrumSession {
public:
    explicit DrumSession(const SongChart& chart):chart_(chart),evaluator_(chart){reset();}
    void reset(){evaluator_.reset();cursor_=0;rebuildWindows();events_.clear();lastUpdate_.reset();hits_=misses_=overhits_=streak_=0;starPowerAvailable_=false;
        disabledActivation_.assign(chart_.notes.size(),false);clearRoll();automaticCount_=0;lastRollStrike_=lastRollCycle_=lastRollEnd_=0;}
    // Same stable active-chart contract as GuitarSession; never called from
    // inside an event callback. Event sinks are transient, not player state.
    void swapRuntimeState(DrumSession& other) noexcept {
        sink_={};other.sink_={};evaluator_.swapRuntimeState(other.evaluator_);
        std::swap(events_,other.events_);
        std::swap(lastUpdate_,other.lastUpdate_);
        std::swap(hits_,other.hits_);
        std::swap(misses_,other.misses_);
        std::swap(overhits_,other.overhits_);
        std::swap(streak_,other.streak_);
        std::swap(threshold_,other.threshold_);
        std::swap(pro_,other.pro_);
        std::swap(lefty_,other.lefty_);
        std::swap(precision_,other.precision_);
        std::swap(deadly_,other.deadly_);
        std::swap(starPowerAvailable_,other.starPowerAvailable_);
        std::swap(cursor_,other.cursor_);
        std::swap(windows_,other.windows_);
        std::swap(disabledActivation_,other.disabledActivation_);
        std::swap(rollMask_,other.rollMask_);
        std::swap(remainingRoll_,other.remainingRoll_);
        std::swap(doubleRoll_,other.doubleRoll_);
        std::swap(rollEnded_,other.rollEnded_);
        std::swap(rollKick_,other.rollKick_);
        std::swap(automaticCount_,other.automaticCount_);
        std::swap(ordinaryCount_,other.ordinaryCount_);
        std::swap(lastRollStrike_,other.lastRollStrike_);
        std::swap(lastRollCycle_,other.lastRollCycle_);
        std::swap(lastRollEnd_,other.lastRollEnd_);
    }
    void beginSection(Seconds time){reset();evaluator_.ignoreNotesBefore(time);}
    void prime() noexcept {lastUpdate_.reset();}
    void setStarPowerAvailable(bool ready) noexcept {starPowerAvailable_=ready;}
    // 0x1820D3C40: crossing half capacity suppresses fills starting within
    // the next four quarter-note beats. Charge must be ready ahead of a fill.
    void starPowerBecameAvailable(std::int64_t tick){
        if(disabledActivation_.size()!=chart_.notes.size())disabledActivation_.assign(chart_.notes.size(),false);
        for(const auto& fill:chart_.drumFills)if(fill.startTick<=tick+4*chart_.resolution && fill.activationNote<disabledActivation_.size())disabledActivation_[fill.activationNote]=true;
    }
    [[nodiscard]] uint32_t noteFlags(size_t index) const noexcept {
        if(index>=chart_.notes.size())return 0;
        return chart_.notes[index].drumFlags&~(isActivation(index)?0u:pc_drums::activation);
    }
    using StepObserver=std::function<void(Seconds)>;
    using EventObserver=std::function<void(const DrumEvent&)>;
    void setOptions(bool pro,bool lefty,bool precision,bool deadly,unsigned threshold){
        pro_=pro;lefty_=lefty;if(precision_!=precision){precision_=precision;rebuildWindows();}deadly_=deadly;threshold_=threshold;
    }
    [[nodiscard]] HitWindow noteWindow(size_t index) const noexcept {return index<windows_.size()?windows_[index]:pc_drums::window(chart_,index,precision_);}
    [[nodiscard]] const InputEvaluator& evaluator() const noexcept{return evaluator_;}
    [[nodiscard]] const std::vector<DrumEvent>& events() const noexcept{return events_;}
    [[nodiscard]] size_t hits() const noexcept{return hits_;}
    [[nodiscard]] size_t misses() const noexcept{return misses_;}
    [[nodiscard]] unsigned streak() const noexcept{return streak_;}
    void update(const DrumInput& input,Seconds time,const StepObserver& before={},const EventObserver& sink={},bool bot=false){
        events_.clear();if(!std::isfinite(time)) return;
        sink_=sink;
        const auto run=[&](const DrumInput& sampled,Seconds at){
            if(before)before(at);
            if(!bot){step(sampled,at);return;}
            DrumInput automatic;
            for(size_t i=cursor_;i<chart_.notes.size();++i){
                if(chart_.notes[i].time>at)break;
                if(evaluator_.state(i)!=NoteState::Pending || chart_.notes[i].time!=at)continue;
                const auto& n=chart_.notes[i];automatic.strikes|=n.frets;
                automatic.velocities[pc_drums::velocityLane(n.frets)]=(n.drumFlags&pc_drums::accent)?static_cast<int>(threshold_):0;
            }
            if(lefty_){automatic.strikes=pc_drums::inputMask(automatic.strikes,true,true);automatic.velocities=pc_drums::inputVelocities(automatic.velocities,true);}
            step(automatic,at);
        };
        if((lastUpdate_ && time>*lastUpdate_) || bot) {
            std::vector<Seconds> boundaries;
            for(size_t i=cursor_;i<chart_.notes.size();++i) {
                if(chart_.notes[i].time>time+0.1)break;
                if(evaluator_.state(i)!=NoteState::Pending)continue;
                if(bot && (!lastUpdate_ || *lastUpdate_<chart_.notes[i].time) && chart_.notes[i].time<time)boundaries.push_back(chart_.notes[i].time);
                auto early=chart_.notes[i].time-noteWindow(i).early;
                if(chart_.notes[i].time-early>noteWindow(i).early)early=std::nextafter(early,INFINITY);
                if(lastUpdate_ && *lastUpdate_<early && early<time)boundaries.push_back(early);
                const auto deadline=pc_drums::lateDeadline(chart_.notes[i].time,noteWindow(i).late);
                if(lastUpdate_ && *lastUpdate_<deadline && deadline<time) boundaries.push_back(deadline);
            }
            std::sort(boundaries.begin(),boundaries.end());
            boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
            for(auto at:boundaries)run({},at);
        }
        run(input,time);lastUpdate_=time;sink_={};
    }
private:
    void rebuildWindows(){
        windows_.resize(chart_.notes.size());
        for(size_t begin=0;begin<chart_.notes.size();){
            size_t end=begin+1;while(end<chart_.notes.size() && chart_.notes[end].time==chart_.notes[begin].time)++end;
            const auto w=pc_drums::window(chart_,begin,precision_);
            for(size_t i=begin;i<end;++i)windows_[i]=w;
            begin=end;
        }
    }
    bool isActivation(size_t index) const noexcept {return (chart_.notes[index].drumFlags&pc_drums::activation) && (index>=disabledActivation_.size() || !disabledActivation_[index]);}
    void emit(DrumEvent event){events_.push_back(event);if(sink_)sink_(event);}
    void activate(size_t index,Seconds time){starPowerAvailable_=false;emit({DrumEventKind::Activate,index,time});}
    void step(const DrumInput& input,Seconds time){
        const std::array<int,8> unknown{-1,-1,-1,-1,-1,-1,-1,-1};
        // 0x1820D37F0: a roll can complete heads automatically at the late
        // deadline. This precedes roll timer expiration in 0x1820D3F90.
        for(size_t begin=cursor_;begin<chart_.notes.size();){
          if(chart_.notes[begin].time>time)break;
          size_t end=begin+1;while(end<chart_.notes.size() && chart_.notes[end].time==chart_.notes[begin].time)++end;
          for(size_t ordinal=0;ordinal<end-begin;++ordinal){
            const size_t index=ordinal+1<end-begin?begin+ordinal+1:begin;
            const auto& n=chart_.notes[index];
            if(evaluator_.state(index)!=NoteState::Pending || n.time-time>=-noteWindow(index).late)continue;
            bool automatic=starPowerAvailable_ && isActivation(index);
            if(!automatic && (n.frets&rollMask_) && automaticCount_<(doubleRoll_?4u:2u)){
                ++automaticCount_;automatic=true;
            }else if(!automatic && (n.drumFlags&pc_drums::doubleRoll) && rollMask_ && !(n.frets&rollMask_)){
                addRoll(n,time);++automaticCount_;automatic=true;
            }else if(!automatic && (n.frets&rollMask_) && time-lastRollStrike_<0.075)automatic=true;
            if(automatic){attemptHit(index,time,unknown,true);continue;}
            if(!(n.frets&1))clearRoll();
            evaluator_.judgeAutomatic(index,NoteState::Missed);++misses_;streak_=0;
            emit({DrumEventKind::Miss,index,pc_drums::lateDeadline(n.time,noteWindow(index).late)});
          }
          begin=end;
        }
        if(rollMask_ && rollEnded_ && time-lastRollEnd_>0.5)clearRoll();
        if(rollMask_ && doubleRoll_ && !rollEnded_ && time-lastRollCycle_>0.75){
            streak_=0;emit({DrumEventKind::Overhit,0,time});clearRoll();
        }
        // Base cursor advances over a chord only after every sibling is judged.
        // Its pre-input value gates overhits (0x1820D4FB0), including extra
        // strikes on the same frame as the first successful chord.
        size_t first=cursor_;
        while(first<chart_.notes.size()){
            size_t end=first+1;while(end<chart_.notes.size() && chart_.notes[end].time==chart_.notes[first].time) ++end;
            bool pending=false;for(size_t i=first;i<end;++i) pending|=evaluator_.state(i)==NoteState::Pending;
            if(pending) break;
            first=end;
        }
        cursor_=first;
        const bool canBreak=first>0 && first<chart_.notes.size();
        const uint16_t mask=pc_drums::inputMask(input.strikes,pro_,lefty_);
        const auto velocities=pc_drums::inputVelocities(input.velocities,lefty_);
        uint16_t consumed=0;std::optional<size_t> activationNote;
        // One fresh strike per physical pad/cymbal/kick selects its nearest
        // pending in-window gem. Repeated-pad misses must not leave every
        // subsequent strike one head behind. Precision keeps PC ordering.
        std::array<std::optional<size_t>,8> preferred{};
        uint16_t chronological=rollMask_;
        if(mask && !precision_)for(size_t i=first;i<chart_.notes.size();++i) {
            const auto& note=chart_.notes[i];if(note.time>time+0.1)break;
            if(evaluator_.state(i)!=NoteState::Pending || !noteWindow(i).contains(note.time,time))continue;
            if(note.drumFlags&(pc_drums::roll|pc_drums::doubleRoll|pc_drums::rollEnd))chronological|=note.frets;
            for(unsigned lane=0;lane<preferred.size();++lane)if(mask&note.frets&(1u<<lane)) {
                auto& best=preferred[lane];
                if(!best || std::abs(note.time-time)<std::abs(chart_.notes[*best].time-time))best=i;
            }
        }
        for(size_t begin=first;begin<chart_.notes.size();){
            if(chart_.notes[begin].time>time+0.1)break;
            size_t end=begin+1;while(end<chart_.notes.size() && chart_.notes[end].time==chart_.notes[begin].time) ++end;
            // PC chord enumeration visits linked children before their root.
            for(size_t ordinal=0;ordinal<end-begin;++ordinal){
                const size_t i=ordinal+1<end-begin?begin+ordinal+1:begin;
                const auto& n=chart_.notes[i];
                if(evaluator_.state(i)!=NoteState::Pending || !noteWindow(i).contains(n.time,time)) continue;
                if(isActivation(i))activationNote=i;
                if(!(mask&~consumed&n.frets))continue;
                // Authored rolls retain the PC automatic/chronological path.
                if(!precision_ && !(n.frets&chronological)) {
                    bool chosen=false;
                    for(unsigned lane=0;lane<preferred.size();++lane)
                        if((mask&~consumed&n.frets&(1u<<lane)) && preferred[lane]==i)chosen=true;
                    if(!chosen)continue;
                }
                if(starPowerAvailable_ && isActivation(i))activate(i,time);
                consumed|=n.frets;
                if(n.drumFlags&(pc_drums::roll|pc_drums::doubleRoll)){
                    automaticCount_=0;consumeRoll(n.frets,time);addRoll(n,time);
                }
                attemptHit(i,time,velocities,false);
            }
            begin=end;
        }
        const auto extra=static_cast<uint16_t>(mask&~consumed);
        if(extra && canBreak){
            if(activationNote && evaluator_.state(*activationNote)==NoteState::Pending && starPowerAvailable_){
                const auto& n=chart_.notes[*activationNote];
                if(n.drumFlags&(pc_drums::roll|pc_drums::doubleRoll)){automaticCount_=0;consumeRoll(n.frets,time);addRoll(n,time);}
                // The PC unmatched-pad branch calls hit directly; unlike a
                // matching activation pad, it does not call BaseEngine activate.
                attemptHit(*activationNote,time,velocities,false);return;
            }
            if((extra&rollMask_)==extra){automaticCount_=0;consumeRoll(extra,time);}
            else if(rollMask_ && doubleRoll_ && !rollKick_ && (extra&1)){rollKick_=true;automaticCount_=0;}
            else if(!(lefty_ && (mask&130)==130 && (extra&130))){
                clearRoll();++overhits_;streak_=0;emit({DrumEventKind::Overhit,0,time});
            }
        }
    }
    void clearRoll() noexcept {rollMask_=remainingRoll_=0;doubleRoll_=rollEnded_=rollKick_=false;ordinaryCount_=0;}
    void addRoll(const Note& note,Seconds time){
        if(rollEnded_)clearRoll();
        if(!(rollMask_&note.frets)){
            rollEnded_=false;doubleRoll_=(note.drumFlags&pc_drums::doubleRoll)!=0;
            rollMask_|=note.frets;remainingRoll_|=note.frets;lastRollCycle_=time;
        }
    }
    void consumeRoll(uint16_t mask,Seconds time){
        remainingRoll_&=~mask;lastRollStrike_=time;
        if(!remainingRoll_){lastRollCycle_=time;remainingRoll_=rollMask_;}
    }
    void attemptHit(size_t index,Seconds time,const std::array<int,8>& velocities,bool automatic){
        const auto& note=chart_.notes[index];
        if(!(note.frets&1) && rollMask_ && !(note.drumFlags&(pc_drums::roll|pc_drums::doubleRoll)) && ++ordinaryCount_>2)clearRoll();
        if(note.drumFlags&pc_drums::rollEnd){rollEnded_=true;lastRollEnd_=time;}
        const auto points=pc_drums::hitPoints(note.drumFlags,note.frets,velocities[pc_drums::velocityLane(note.frets)],threshold_,deadly_);
        if(!points)return;
        const auto windows=[&](size_t i,const Note&){return noteWindow(i);};
        const bool hit=automatic?evaluator_.judgeAutomatic(index,NoteState::Hit):
            evaluator_.tryHit(index,time,windows,[](const Note&){return true;}).has_value();
        if(hit){++hits_;++streak_;emit({DrumEventKind::Hit,index,time,*points});}
    }
    const SongChart& chart_;
    InputEvaluator evaluator_;
    std::vector<DrumEvent> events_;
    std::optional<Seconds> lastUpdate_;
    size_t hits_{},misses_{},overhits_{};
    unsigned streak_{},threshold_{100};
    bool pro_{},lefty_{},precision_{},deadly_{},starPowerAvailable_{};
    EventObserver sink_;
    size_t cursor_{};std::vector<HitWindow> windows_;
    std::vector<bool> disabledActivation_;
    uint16_t rollMask_{},remainingRoll_{};
    bool doubleRoll_{},rollEnded_{},rollKick_{};
    unsigned automaticCount_{},ordinaryCount_{};
    Seconds lastRollStrike_{},lastRollCycle_{},lastRollEnd_{};
};
}
