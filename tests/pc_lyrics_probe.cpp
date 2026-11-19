// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "../examples/cloneheronative/gameplay/pc_lyrics.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
int main() {
    std::vector<rhythm::LyricEvent> events;std::string line;
    while(std::getline(std::cin,line)) {
        std::istringstream in(line);rhythm::LyricEvent e;
        if(!(in>>e.time>>e.kind))return 2;
        std::getline(in,e.text);if(!e.text.empty()&&e.text.front()==' ')e.text.erase(0,1);
        events.push_back(std::move(e));
    }
    std::cout<<std::setprecision(17);
    for(const auto& phrase:rhythm::pcLyricPhrases(std::move(events))) {
        std::cout<<"P\t"<<phrase.start<<'\t'<<phrase.end<<'\t'<<phrase.text<<'\n';
        for(const auto& s:phrase.syllables)std::cout<<"S\t"<<s.time<<'\t'<<s.end<<'\n';
    }
}
