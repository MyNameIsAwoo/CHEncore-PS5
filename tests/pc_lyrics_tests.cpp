// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "../examples/cloneheronative/gameplay/pc_lyrics.hpp"
#include <cassert>
#include <cstdio>
int main() {
    using rhythm::LyricEvent;
    auto phrases=rhythm::pcLyricPhrases({
        {1,0,{}},{1.2,2,"Some-"},{1.5,2,"thing"},{2,2,"new#"},{3,1,{}},
        {5,0,{}},{5,2,"<b>Hello</b>_world="},{5.5,2,"again^"},{6,2,"+"},{7,1,{}},
        {8,0,{}},{8.2,2,"unclosed"}});
    assert(phrases.size()==2);
    assert(phrases[0].text=="Something new");
    assert(phrases[1].text=="Hello world-again");
    auto v=rhythm::pcLyricView(phrases,1.3);
    assert(v.current==0&&v.next==1&&v.highlighted==4&&v.alpha==1);
    v=rhythm::pcLyricView(phrases,2.1);assert(v.highlighted==13);
    v=rhythm::pcLyricView(phrases,3.2);assert(v.alpha>0&&v.alpha<1);
    v=rhythm::pcLyricView(phrases,3.5);assert(v.current==-1&&v.next==-1);
    v=rhythm::pcLyricView(phrases,4.5);assert(v.current==1&&v.highlighted==0);
    // Restart/practice seek has no mutable cursor to retain the old line.
    v=rhythm::pcLyricView(phrases,-2);assert(v.current==-1&&v.next==-1);
    v=rhythm::pcLyricView(phrases,1.2);assert(v.current==0&&v.highlighted==4);
    auto boundary=rhythm::pcLyricPhrases({{0,0,{}},{0,2,"first"},{1,2,"second"},{1,0,{}},{2,1,{}}});
    assert(boundary.size()==2&&boundary[0].text=="first"&&boundary[1].text=="second");
    assert(rhythm::pcLyricPhrases({{0,0,{}},{1,1,{}}}).empty());
    auto consecutive=rhythm::pcLyricPhrases({{1,0,{}},{1,2,"one"},{2,1,{}},
        {2,0,{}},{2,2,"two"},{3,1,{}}});
    auto rising=rhythm::pcLyricView(consecutive,1.5);
    assert(rising.current==1 && rising.promotion==0 && rising.outgoing==0 && rising.outgoingAlpha==1);
    rising=rhythm::pcLyricView(consecutive,1.7);
    assert(rising.promotion>.49f && rising.promotion<.51f && rising.outgoingAlpha>.49f);
    rising=rhythm::pcLyricView(consecutive,1.95);
    assert(rising.promotion==1 && rising.outgoing==-1);
    assert(rhythm::pcLyricView(consecutive,1.7).promotion==rhythm::pcLyricView(consecutive,1.7).promotion);
    assert(rhythm::pcLyricView(consecutive,1.5).promotion==0); // backwards seek
    std::puts("PASS lyrics: phrases, syllables, control characters, boundaries, fade and seek");
}
