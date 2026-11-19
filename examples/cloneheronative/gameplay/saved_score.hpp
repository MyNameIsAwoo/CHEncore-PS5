// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
namespace rhythm {
struct SavedScore {
    uint64_t score{};
    unsigned stars{},accuracyHundredths{};
    int instrument{},difficulty{},controller{};
    bool details{},accuracyKnown{};
    std::string player;
};
inline bool betterSavedScore(const SavedScore& value,const SavedScore& previous) noexcept {
    if(value.score!=previous.score)return value.score>previous.score;
    if(value.stars!=previous.stars)return value.stars>previous.stars;
    if(value.details!=previous.details)return value.details;
    if(value.accuracyKnown!=previous.accuracyKnown)return value.accuracyKnown;
    return value.accuracyKnown && value.accuracyHundredths>previous.accuracyHundredths;
}
// Version one stores only points. Never invent stars or ownership for old scores.
inline bool readSavedScore(FILE* file,SavedScore& result) {
    char magic[24]{};unsigned version{};unsigned long long points{};
    if(!file || std::fscanf(file,"%23s %u %llu",magic,&version,&points)!=3 ||
       std::strcmp(magic,"CHPS5SCORE") || (version!=1 && version!=2 && version!=3))return false;
    SavedScore value;value.score=points;
    if(version>=2) {
        if(std::fscanf(file,"%u %d",&value.stars,&value.controller)!=2 || value.stars>7 ||
           value.controller<0 || value.controller>4)return false;
        if(version>=3){if(std::fscanf(file,"%u",&value.accuracyHundredths)!=1 || value.accuracyHundredths>10000)return false;value.accuracyKnown=true;}
        int c=std::fgetc(file);if(c=='\r')c=std::fgetc(file);if(c!='\n')return false;
        char name[256]{};if(!std::fgets(name,sizeof(name),file))return false;
        const auto length=std::strcspn(name,"\r\n");
        if(!length || length>192)return false;
        value.player.assign(name,length);value.details=true;
    }
    result=std::move(value);return true;
}
inline bool writeSavedScore(FILE* file,const SavedScore& value) {
    if(!file || value.stars>7 || value.controller<0 || value.controller>4 || value.accuracyHundredths>10000 ||
       value.player.empty() || value.player.size()>192 || value.player.find_first_of("\r\n")!=std::string::npos)return false;
    return std::fprintf(file,"CHPS5SCORE 3\n%llu %u %d %u\n%s\n",
        static_cast<unsigned long long>(value.score),value.stars,value.controller,value.accuracyHundredths,value.player.c_str())>0;
}
}
