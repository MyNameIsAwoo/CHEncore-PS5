// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include "midi_drums.hpp"
#include <string>
namespace rhythm::midi {
struct Profile {
    uint16_t vid{},pid{};
    std::string name;
    bool twoKickPads{};
    Mapping mapping{};
};
bool parseProfile(const std::string& text,Profile& out,std::string& error);
std::string writeProfile(const Profile& profile);
bool saveProfile(const char* path,const Profile& profile,std::string& error);
bool loadProfile(const char* path,Profile& profile,std::string& error);
}
