// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
namespace rhythm {
struct LyricEvent { double time{}; unsigned kind{}; std::string text; }; // start=0, end=1, syllable=2
struct LyricSyllable { double time{}; size_t end{}; };
struct LyricPhrase { double start{},end{}; std::string text; std::vector<LyricSyllable> syllables; };
// PC phrase converter 0x1820BA4A0 closes a start at the next end/start,
// drops unterminated/empty phrases, and assigns a shared boundary to the next
// phrase. 0x18214FD50 joins syllables and removes vocal control characters.
inline std::vector<LyricPhrase> pcLyricPhrases(std::vector<LyricEvent> events) {
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    std::vector<LyricPhrase> out;
    for(size_t i=0;i<events.size();++i) {
        if(events[i].kind!=0 || !std::isfinite(events[i].time))continue;
        size_t end=i+1;
        while(end<events.size() && events[end].kind==2)++end;
        if(end==events.size() || events[end].time<events[i].time)continue;
        LyricPhrase phrase{events[i].time,events[end].time,{},{}};
        bool joined=false;
        const auto first=std::lower_bound(events.begin(),events.end(),phrase.start,
            [](const LyricEvent& e,double time){return e.time<time;});
        for(size_t j=static_cast<size_t>(first-events.begin());j<events.size() && events[j].time<=phrase.end;++j) {
            const auto& event=events[j];
            if(event.kind!=2 || event.text.empty() || event.text=="+")continue;
            // A syllable at another phrase's start belongs to that phrase.
            if(events[end].kind==0 && event.time==events[end].time)continue;
            std::string word;bool tag=false;
            for(size_t k=0;k<event.text.size();++k) {
                const unsigned char c=event.text[k];
                if(c=='<' && event.text.find('>',k)!=std::string::npos){tag=true;continue;}
                if(tag){if(c=='>')tag=false;continue;}
                if(c=='#'||c=='$'||c=='%'||c=='*'||c=='+'||c=='-'||c=='/'||c=='^')continue;
                if(c=='=' )word+='-';
                else if(c=='_')word+=' ';
                else if(c==0xc2 && k+1<event.text.size() && static_cast<unsigned char>(event.text[k+1])==0xa7){word+=' ';++k;}
                else if(c>=32)word+=static_cast<char>(c);
            }
            if(!word.empty()) {
                if(!phrase.text.empty()&&!joined && event.text.front()!='+')phrase.text+=' ';
                phrase.text+=word;phrase.syllables.push_back({event.time,phrase.text.size()});
            }
            joined=event.text.find('-')!=std::string::npos || event.text.ends_with('=');
        }
        if(!phrase.text.empty())out.push_back(std::move(phrase));
    }
    return out;
}
// LyricDisplay 0x18023EAB0 uses a 0.4s normal transition; LyricAnimator
// 0x1802DD5E0 linearly interpolates positions/alpha. All animation follows
// the song clock, so freezing or rewinding it freezes or rewinds the lyrics.
inline constexpr double pcLyricTransition=.4;
struct LyricView {
    int current{-1},next{-1},outgoing{-1};size_t highlighted{};
    float alpha{},promotion{1},outgoingAlpha{};
};
// Stateless lookup follows the song clock: pause, seek, restart and practice
// cannot leave stale phrase cursors. No text allocation in the render loop.
inline LyricView pcLyricView(const std::vector<LyricPhrase>& phrases,double time) noexcept {
    LyricView view;
    // LyricDisplay.Update 0x180241E30 advances half a second before start.
    const auto next=std::upper_bound(phrases.begin(),phrases.end(),time+.5,
        [](double t,const LyricPhrase& p){return t<p.start;});
    if(next!=phrases.end())view.next=static_cast<int>(next-phrases.begin());
    if(next!=phrases.begin()) {
        const auto& current=*(next-1);
        if(time<current.end+.4) {
            view.current=static_cast<int>(next-phrases.begin()-1);
            view.promotion=static_cast<float>(std::clamp((time-(current.start-.5))/pcLyricTransition,0.0,1.0));
            if(view.current>0 && view.promotion<1 && current.start-phrases[view.current-1].end<2.5) {
                view.outgoing=view.current-1;view.outgoingAlpha=1-view.promotion;
            }
            view.alpha=static_cast<float>(std::clamp((current.end+.4-time)/.4,0.0,1.0));
            const auto word=std::upper_bound(current.syllables.begin(),current.syllables.end(),time,
                [](double t,const LyricSyllable& s){return t<s.time;});
            if(word!=current.syllables.begin())view.highlighted=(word-1)->end;
        }
    }
    if(view.current>=0 && view.next>=0 && phrases[view.next].start-phrases[view.current].end>=2.5)
        view.next=-1;
    // PC LyricDisplay.Update introduces an upcoming phrase before its start;
    // avoid filling instrumental breaks with a distant line.
    if(view.current<0 && view.next>=0 && phrases[view.next].start-time>.5)view.next=-1;
    return view;
}
}
