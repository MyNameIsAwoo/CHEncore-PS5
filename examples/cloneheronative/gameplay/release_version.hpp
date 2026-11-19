// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <string>
#include <utility>
namespace rhythm::updates {
struct ReleaseVersion {unsigned code{};bool beta{};std::string label,prefix;};
// Preserve ordering from the old alpha channel through betas and stable builds.
// For a given semantic version, stable follows beta. A newer patch/minor wins
// regardless of its phase. Code fits the PS5 01.xxx.xxx content-version fields.
inline bool releaseVersion(const std::string& tag,ReleaseVersion& result) {
    if(tag.empty() || tag.size()>32)return false;
    ReleaseVersion version;
    if(tag.rfind("alpha-v",0)==0) {
        const auto digits=tag.substr(7);if(digits.empty() || digits.size()>3 || digits[0]=='0')return false;
        for(char c:digits){if(c<'0'||c>'9')return false;version.code=version.code*10+unsigned(c-'0');}
        if(!version.code)return false;version.label="Alpha v."+digits;version.prefix="CH_Encore_Alpha_v"+digits;
    } else {
        const bool beta=tag.rfind("beta-v",0)==0;
        if(!beta && tag.front()!='v')return false;
        const auto number=tag.substr(beta?6:1);std::array<unsigned,3> parts{};unsigned count=0;size_t start=0;
        while(start<number.size()) {
            if(count==parts.size())return false;const auto end=number.find('.',start);const auto digits=number.substr(start,end==std::string::npos?end:end-start);
            if(digits.empty() || digits.size()>2 || (digits.size()>1 && digits.front()=='0'))return false;
            for(char c:digits){if(c<'0'||c>'9')return false;parts[count]=parts[count]*10+unsigned(c-'0');}
            ++count;if(end==std::string::npos)break;start=end+1;if(start==number.size())return false;
        }
        if(count<2 || !parts[0] || parts[0]>99 || parts[1]>99 || parts[2]>49)return false;
        version.code=parts[0]*10000+parts[1]*100+parts[2]*2+(beta?0:1);version.beta=beta;
        version.label=(beta?"Beta ":"")+number;version.prefix="CH_Encore_"+std::string(beta?"Beta_":"")+number;
    }
    result=std::move(version);return true;
}
}
