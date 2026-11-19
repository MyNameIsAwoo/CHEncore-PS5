// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "midi_profile.hpp"
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#define JSON_NOEXCEPTION 1
#include "json.hpp"
namespace rhythm::midi {
namespace {
using Json=nlohmann::json;
constexpr const char* actionNames[]{"kick","pad_red","pad_yellow","cymbal_yellow","pad_blue","cymbal_blue","pad_green","cymbal_green","cymbal_orange"};
bool number(const Json& value,const char* key,unsigned max,unsigned& out){
    const auto i=value.find(key);if(i==value.end() || !i->is_number_integer())return false;
    const auto v=i->get<int64_t>();if(v<0 || uint64_t(v)>max)return false;out=unsigned(v);return true;
}
bool nameValid(const std::string& name){
    if(name.empty() || name.size()>128)return false;
    for(unsigned char c:name)if(c<32 || c==127)return false;
    return true;
}
}
bool parseProfile(const std::string& text,Profile& out,std::string& error){
    error="Invalid MIDI controller profile.";
    if(text.size()>65536)return false;
    const auto j=Json::parse(text,nullptr,false);
    if(!j.is_object() || !j.count("schema") || !j["schema"].is_number_integer() || j["schema"]!=1 ||
       !j.count("source") || j["source"]!="usb_midi" ||
       !j.count("name") || !j["name"].is_string() ||
       !j.count("instrument") || !j["instrument"].is_string() ||
       !j.count("two_kick_pads") || !j["two_kick_pads"].is_boolean() ||
       !j.count("bindings") || !j["bindings"].is_array() || j["bindings"].empty() ||
       j["bindings"].size()>Mapping::capacity)return false;
    const auto instrument=j["instrument"].get<std::string>();
    if(instrument!="drums_pro" && instrument!="drums_5lane")return false;
    Profile next;next.mapping=Mapping(instrument=="drums_pro"?Layout::Pro4:Layout::FiveLane);
    unsigned vid,pid;
    if(!number(j,"vid",65535,vid) || !number(j,"pid",65535,pid))return false;
    next.vid=uint16_t(vid);next.pid=uint16_t(pid);next.name=j["name"].get<std::string>();
    if(!nameValid(next.name))return false;
    next.twoKickPads=j["two_kick_pads"].get<bool>();
    for(const auto& item:j["bindings"]){
        if(!item.is_object() || !item.count("action") || !item["action"].is_string())return false;
        const auto name=item["action"].get<std::string>();unsigned action=0;
        while(action<std::size(actionNames) && name!=actionNames[action])++action;
        if(action==std::size(actionNames))return false;
        unsigned cable,channel,note,minimum;
        if(!number(item,"cable",15,cable) || !number(item,"channel",15,channel) ||
           !number(item,"note",127,note))return false;
        Binding binding;binding.action=Action(action);binding.cable=uint8_t(cable);
        binding.channel=uint8_t(channel);binding.note=uint8_t(note);
        if(binding.action==Action::Kick){
            if(item.count("velocity") || item.count("minimum_velocity"))return false;
        }else{
            if(!number(item,"minimum_velocity",127,minimum))return false;
            binding.minimumVelocity=uint8_t(minimum);
            if(item.count("velocity")){
                const auto& range=item["velocity"];unsigned soft,hard;
                if(!range.is_object() || !number(range,"soft",127,soft) || !number(range,"hard",127,hard) ||
                   !binding.velocity.capture(soft,hard))return false;
            }
        }
        if(!next.mapping.bind(binding))return false;
    }
    out=std::move(next);error.clear();return true;
}
std::string writeProfile(const Profile& profile){
    Json j={{"schema",1},{"source","usb_midi"},{"vid",profile.vid},{"pid",profile.pid},
        {"name",profile.name},{"instrument",profile.mapping.layout()==Layout::Pro4?"drums_pro":"drums_5lane"},
        {"two_kick_pads",profile.twoKickPads},{"bindings",Json::array()}};
    for(const auto& binding:profile.mapping.bindings()){
        if(unsigned(binding.action)>=std::size(actionNames))return {};
        Json item={{"action",actionNames[unsigned(binding.action)]},{"cable",binding.cable},
            {"channel",binding.channel},{"note",binding.note}};
        if(binding.action!=Action::Kick){
            item["minimum_velocity"]=binding.minimumVelocity;
            if(binding.velocity.calibrated())item["velocity"]={{"soft",binding.velocity.soft},{"hard",binding.velocity.hard}};
        }
        j["bindings"].push_back(std::move(item));
    }
    return j.dump(2)+'\n';
}
bool saveProfile(const char* path,const Profile& profile,std::string& error){
    const auto text=writeProfile(profile);Profile checked;
    if(!path || !*path || !parseProfile(text,checked,error))return false;
    const auto temporary=std::string(path)+".tmp";
    const int fd=open(temporary.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0666);
    FILE* f=fd<0?nullptr:fdopen(fd,"wb");
    if(!f){if(fd>=0)close(fd);error="Could not create MIDI controller profile.";return false;}
    bool ok=std::fwrite(text.data(),1,text.size(),f)==text.size() && std::fflush(f)==0 && fsync(fileno(f))==0;
    if(std::fclose(f))ok=false;
    if(ok)ok=std::rename(temporary.c_str(),path)==0;
    if(!ok){(void)unlink(temporary.c_str());error="Could not save MIDI controller profile.";return false;}
    error.clear();return true;
}
bool loadProfile(const char* path,Profile& profile,std::string& error){
    if(!path || !*path){error="MIDI controller profile path is missing.";return false;}
    const int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
    FILE* f=fd<0?nullptr:fdopen(fd,"rb");
    if(!f){if(fd>=0)close(fd);error="MIDI controller profile was not found.";return false;}
    struct stat st{};
    bool ok=fstat(fileno(f),&st)==0 && S_ISREG(st.st_mode) && st.st_size>0 && st.st_size<=65536;
    std::string text(ok?size_t(st.st_size):0,'\0');
    if(ok)ok=std::fread(text.data(),1,text.size(),f)==text.size();std::fclose(f);
    if(!ok){error="Could not read MIDI controller profile.";return false;}
    return parseProfile(text,profile,error);
}
}
