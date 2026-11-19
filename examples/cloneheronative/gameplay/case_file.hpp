// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <cctype>
#include <cstdio>
#include <dirent.h>
#include <string>
namespace rhythm {
// Song packages come from case-insensitive PC filesystems. Preserve their
// original filenames while resolving standard metadata/art names on the PS5.
inline std::string caseFile(const std::string& folder,const std::string& name) {
    const std::string exact=folder+"/"+name;
    if(FILE* f=std::fopen(exact.c_str(),"rb")){std::fclose(f);return exact;}
    DIR* d=opendir(folder.c_str());if(!d)return {};
    std::string result;
    while(auto* entry=readdir(d)) {
        const std::string actual=entry->d_name;if(actual.size()!=name.size())continue;
        bool same=true;for(size_t i=0;i<name.size();++i)
            if(std::tolower(static_cast<unsigned char>(actual[i]))!=std::tolower(static_cast<unsigned char>(name[i]))){same=false;break;}
        if(same){result=folder+"/"+actual;break;}
    }
    closedir(d);return result;
}
}
