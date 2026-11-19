// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
namespace rhythm {
// Platform-independent text editor for raw instruments that cannot drive the
// system IME. The UI owns opening/closing and commits only on explicit accept.
struct InstrumentKeyboard {
    bool open{},upper{};
    int key{};
    int trackedKey{};
    float preferredColumn{.5f};
    std::size_t limit{96};
    std::string text;
    static constexpr const char* characters="1234567890qwertyuiopasdfghjkl'zxcvbnm,.-";
    static constexpr int keyCount=44;
    void begin(std::string initial,std::size_t maximum) {
        text=std::move(initial);limit=maximum;key=trackedKey=0;preferredColumn=.5f;upper=false;open=true;
        while(text.size()>limit)erase();
    }
    void erase() noexcept {
        if(text.empty())return;
        std::size_t start=text.size()-1;
        while(start && (static_cast<unsigned char>(text[start])&0xc0)==0x80)--start;
        text.erase(start);
    }
    static constexpr float actionColumns[]{0,5,6.5f,8};
    static constexpr float actionWidths[]{5,1.5f,1.5f,2};
    static float centerColumn(int index) noexcept {
        return index<40?float(index%10)+.5f:actionColumns[index-40]+actionWidths[index-40]*.5f;
    }
    void setKey(int index) noexcept {key=trackedKey=index;preferredColumn=centerColumn(index);}
    void move(int delta) noexcept {
        const int start=key<40?(key/10)*10:40,count=key<40?10:4;
        setKey(start+(key-start+delta+count)%count);
    }
    void strum(int delta) noexcept {setKey((key+delta+keyCount)%keyCount);}
    void row(int delta) noexcept {
        if(key!=trackedKey)preferredColumn=centerColumn(key);
        const int current=key<40?key/10:4;
        const int next=(current+delta+5)%5;
        if(next<4)key=next*10+std::clamp(int(preferredColumn),0,9);
        else {
            int action=0;
            while(action<3 && preferredColumn>=actionColumns[action+1])++action;
            key=40+action;
        }
        trackedKey=key;
    }
    char letter(int index) const noexcept {
        if(index<0 || index>=40)return 0;
        const char c=characters[index];return upper && c>='a' && c<='z'?char(c-'a'+'A'):c;
    }
    // Returns 1 for Done, -1 for Cancel; all other keys edit the draft only.
    int select() {
        if(key==42)return 1;
        if(key==43)return -1;
        if(key==41){text.clear();return 0;}
        const char c=key==40?' ':letter(key);
        if(c && text.size()<limit)text.push_back(c);
        return 0;
    }
};
}
