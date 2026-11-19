// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "controller_profile.hpp"
#include "controller_navigation.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif
#define JSON_NOEXCEPTION 1
#include "json.hpp"
namespace rhythm::controls {
using Json=nlohmann::json;
namespace {
bool validUtf8(const std::string& text){
    for(size_t i=0;i<text.size();) {
        const auto first=static_cast<unsigned char>(text[i++]);if(first<128)continue;
        unsigned count=first>=0xc2 && first<=0xdf?1:first>=0xe0 && first<=0xef?2:first>=0xf0 && first<=0xf4?3:0;
        if(!count || i+count>text.size())return false;
        uint32_t value=first&((1u<<(6-count))-1);
        for(unsigned n=0;n<count;++n){const auto c=static_cast<unsigned char>(text[i++]);if((c&0xc0)!=0x80)return false;value=(value<<6)|(c&63);}
        if((count==1 && value<0x80)||(count==2 && value<0x800)||(count==3 && value<0x10000)||value>0x10ffff||(value>=0xd800 && value<=0xdfff))return false;
    }return true;
}
bool commitFile(FILE* file){
    if(std::fflush(file))return false;
#if defined(_WIN32)
    return _commit(_fileno(file))==0;
#else
    return fsync(fileno(file))==0;
#endif
}
constexpr const char* buttonNames[]{"cross","circle","square","triangle","l1","r1","l2","r2","touchpad","up","down","options","right","left"};
constexpr const char* actions5[]{"green","red","yellow","blue","orange","strum_up","strum_down","start","star_power"};
constexpr const char* actions6[]{"w1","w2","w3","b1","b2","b3","strum_up","strum_down","start","star_power"};
constexpr const char* actions4d[]{"kick","pad_red","pad_yellow","pad_blue","pad_green","start","star_power"};
constexpr const char* actions5d[]{"kick","pad_green","pad_red","pad_yellow","pad_blue","pad_orange","start","star_power"};
constexpr const char* actionsPro[]{"kick","pad_red","pad_yellow","cymbal_yellow","pad_blue","cymbal_blue","pad_green","cymbal_green","start","star_power"};
const char* actionName(int type,unsigned i){return type==4?actionsPro[i]:type==0?actions5[i]:type==1?actions6[i]:type==2?actions4d[i]:actions5d[i];}
bool integer(const Json& j,const char* key,int low,int high,int& value){
    auto it=j.find(key);if(it==j.end() || !it->is_number_integer())return false;
    const auto n=it->get<int64_t>();if(n<low || n>high)return false;value=int(n);return true;
}
bool number(const Json& j,const char* key,float& out){auto it=j.find(key);if(it==j.end() || !it->is_number())return false;out=it->get<float>();return std::isfinite(out);}
bool text(const Json& j,const char* key,std::string& out){auto it=j.find(key);if(it==j.end() || !it->is_string())return false;out=it->get<std::string>();return out.size()<=128;}
bool hex(const Json& j,const char* key,uint32_t& out){std::string s;if(!text(j,key,s) || s.size()<3 || s.compare(0,2,"0x"))return false;auto r=std::from_chars(s.data()+2,s.data()+s.size(),out,16);return r.ec==std::errc{} && r.ptr==s.data()+s.size();}
const char* axisName(unsigned i){constexpr const char* names[]{"l2","r2","lx","ly","rx","ry"};return names[i];}
int axisIndex(const std::string& name){for(int i=0;i<6;++i)if(name==axisName(i))return i;return -1;}
bool locator(const Json& j,Source source,unsigned length,Locator& l){
    if(!j.is_object())return false;std::string type;if(!text(j,"type",type))return false;
    if(type=="digital")l.kind=Kind::Digital;else if(type=="hat")l.kind=Kind::Hat;else if(type=="analog")l.kind=Kind::Analog;else return false;
    if(source==Source::Native){
        if(l.kind==Kind::Digital){if(!text(j,"pad_button",l.padButton) || !nativeButton(l.padButton))return false;}
        else if(l.kind==Kind::Analog){if(!text(j,"pad_axis",l.padAxis) || axisIndex(l.padAxis)<0)return false;}
        else return false;
    } else {
        if(!integer(j,"byte",0,int(length)-1,l.byte))return false;
        if(l.kind==Kind::Digital){if(!hex(j,"mask",l.mask) || !l.mask || l.mask>255)return false;}
        else if(l.kind==Kind::Hat){if(!integer(j,"value",0,255,l.value))return false;
            if(j.find("mask")!=j.end() && (!hex(j,"mask",l.mask)||!l.mask||l.mask>255))return false;}
        else if(!integer(j,"bytes",1,2,l.bytes) || unsigned(l.byte+l.bytes)>length)return false;
    }
    if(l.kind==Kind::Analog){
        if(!number(j,"rest",l.rest)||!number(j,"min",l.min)||!number(j,"max",l.max)||l.min>=l.max||l.rest<l.min||l.rest>l.max)return false;
        auto i=j.find("invert");if(i!=j.end()){if(!i->is_boolean())return false;l.invert=i->get<bool>();}
    }
    return true;
}
std::string hexString(unsigned n){char b[16];std::snprintf(b,sizeof(b),"0x%04x",n);return b;}
Json serialize(const Locator& l,Source source){
    Json j={{"type",l.kind==Kind::Digital?"digital":l.kind==Kind::Hat?"hat":"analog"}};
    if(source==Source::Native){if(l.kind==Kind::Digital)j["pad_button"]=l.padButton;else j["pad_axis"]=l.padAxis;}
    else {j["byte"]=l.byte;if(l.kind==Kind::Digital)j["mask"]=hexString(l.mask);else if(l.kind==Kind::Hat){j["value"]=l.value;if(l.mask)j["mask"]=hexString(l.mask);}else j["bytes"]=l.bytes;}
    if(l.kind==Kind::Analog){j["rest"]=l.rest;j["min"]=l.min;j["max"]=l.max;j["invert"]=l.invert;}
    return j;
}
float sample(const Locator& l,Source source,const NativeSample& n,const uint8_t* raw,unsigned size) noexcept {
    float v=0;
    if(source==Source::Native){if(l.kind==Kind::Digital)return (n.buttons&nativeButton(l.padButton))!=0;const int i=axisIndex(l.padAxis);if(i<0)return 0;v=n.axes[i];}
    else {if(!raw || unsigned(l.byte+l.bytes)>size)return 0;
        if(l.kind==Kind::Digital)return (raw[l.byte]&l.mask)==l.mask;
        if(l.kind==Kind::Hat){
            const int value=raw[l.byte]&(l.mask?l.mask:255);
            if(l.value>=0 && l.value<8 && (l.value%2)==0 && value<8) {
                const int distance=std::abs(value-l.value);return std::min(distance,8-distance)<=1;
            }
            return value==l.value;
        }
        v=raw[l.byte];if(l.bytes==2)v+=raw[l.byte+1]*256.0f;
    }
    // Endpoint-rest calibration: move away from rest towards the opposite end.
    const float span=l.invert?l.rest-l.min:l.max-l.rest;
    if(span<=0)return 0;
    return std::clamp((l.invert?l.rest-v:v-l.rest)/span,0.0f,1.0f);
}
}
uint32_t nativeButton(const std::string& name) noexcept {for(unsigned i=0;i<buttons.size();++i)if(name==buttonNames[i])return buttons[i];return name=="ps"?0x10000:0;}
const char* instrumentName(int type) noexcept {constexpr const char* names[]{"guitar_5fret","guitar_6fret","drums_4lane","drums_5lane","drums_pro"};return names[std::clamp(type,0,4)];}
bool parseProfile(const std::string& data,ControllerProfile& out,std::string& error){
    error.clear();const auto fail=[&](const char* message){error=message;return false;};
    if(data.size()>65536)return fail("Controller profile is too large");
    const auto j=Json::parse(data,nullptr,false);if(j.is_discarded() || !j.is_object())return fail("Invalid controller JSON");
    ControllerProfile p;int schema;if(!integer(j,"schema",1,3,schema))return fail("Unsupported controller schema");p.schema=unsigned(schema);
    auto device=j.find("device");if(device==j.end() || !device->is_object())return fail("Missing device identity");
    uint32_t vid,pid;std::string source;
    if(!hex(*device,"vid",vid)||vid>65535||!hex(*device,"pid",pid)||pid>65535||!text(*device,"name",p.name)||!text(*device,"source",source))return fail("Invalid device identity");
    p.vid=uint16_t(vid);p.pid=uint16_t(pid);
    if(source=="native")p.source=Source::Native;else if(source=="raw_hid")p.source=Source::RawHid;else return fail("Unknown input source");
    if(!text(j,"instrument",p.instrument) || (p.instrument!="guitar_5fret" && p.instrument!="guitar_6fret" && p.instrument!="drums_4lane" && p.instrument!="drums_pro" && p.instrument!="drums_5lane"))return fail("Unknown instrument class");
    if(p.source==Source::RawHid){auto report=j.find("report");int length;if(report==j.end()||!integer(*report,"length",1,64,length))return fail("Invalid report length");p.reportLength=unsigned(length);}
    if(p.source==Source::RawHid && p.schema>=2) {
        const auto& report=j["report"];
        if(report.find("id")!=report.end() && !integer(report,"id",0,255,p.reportId))return fail("Invalid report ID");
        if(auto menu=j.find("menu");menu!=j.end()) {
            if(!menu->is_object())return fail("Invalid menu navigation");
            for(auto item:{std::pair{"x",&p.menuX},std::pair{"y",&p.menuY},std::pair{"hat",&p.menuHat}})
                if(menu->find(item.first)!=menu->end() && !integer(*menu,item.first,0,int(p.reportLength)-1,*item.second))return fail("Invalid menu byte");
            if(menu->find("hat_mask")!=menu->end() && (!hex(*menu,"hat_mask",p.menuHatMask)||!p.menuHatMask||p.menuHatMask>255))return fail("Invalid menu hat mask");
            if(auto guard=menu->find("hat_ignore_when");guard!=menu->end()) {
                if(p.schema<3 || !locator(*guard,p.source,p.reportLength,p.hatSuppression) || p.hatSuppression.kind!=Kind::Digital)return fail("Invalid menu hat guard");
                p.suppressHat=true;
            }
            if(auto buttons=menu->find("buttons");buttons!=menu->end()) {
                if(p.schema<3 || !buttons->is_array() || buttons->size()>16)return fail("Invalid menu buttons");
                constexpr const char* actions[]{"green","red","yellow","blue","orange","start","select","up","down","left","right"};
                for(const auto& entry:*buttons) {
                    Binding binding;
                    if(!text(entry,"action",binding.action) || std::find(std::begin(actions),std::end(actions),binding.action)==std::end(actions) ||
                        !locator(entry,p.source,p.reportLength,binding.input) || binding.input.kind!=Kind::Digital)return fail("Invalid menu button");
                    for(const auto& old:p.menuBindings)if(old.action==binding.action)return fail("Duplicate menu action");
                    if(auto alternatives=entry.find("alternatives");alternatives!=entry.end()) {
                        if(!alternatives->is_array() || alternatives->size()>4)return fail("Invalid menu alternatives");
                        for(const auto& item:*alternatives){Locator input;if(!locator(item,p.source,p.reportLength,input) || input.kind!=Kind::Digital)return fail("Invalid menu alternative");binding.alternatives.push_back(input);}
                    }
                    p.menuBindings.push_back(std::move(binding));
                }
            }
        }
    }
    auto bindings=j.find("bindings");if(bindings==j.end() || !bindings->is_array() || bindings->empty() || bindings->size()>32)return fail("Invalid bindings");
    constexpr const char* allowed[]{"green","red","yellow","blue","orange","w1","w2","w3","b1","b2","b3","strum_up","strum_down","start","star_power","whammy","tilt","kick","pad_red","pad_yellow","pad_blue","pad_green","pad_orange","cymbal_yellow","cymbal_blue","cymbal_green"};
    for(const auto& entry:*bindings){Binding b;if(!text(entry,"action",b.action)||std::find(std::begin(allowed),std::end(allowed),b.action)==std::end(allowed)||!locator(entry,p.source,p.reportLength,b.input))return fail("Invalid binding locator");
        const bool common=b.action=="start" || b.action=="star_power";
        const bool guitar=p.instrument.find("guitar_")==0;
        const bool guitarCommon=b.action=="strum_up" || b.action=="strum_down" || b.action=="whammy" || (p.instrument=="guitar_5fret" && b.action=="tilt");
        bool instrumentAction=false;
        if(p.instrument=="guitar_5fret")for(unsigned i=0;i<5;++i)instrumentAction|=b.action==actions5[i];
        else if(p.instrument=="guitar_6fret")for(unsigned i=0;i<6;++i)instrumentAction|=b.action==actions6[i];
        else if(b.action=="kick" || b.action=="pad_red" || b.action=="pad_yellow" || b.action=="pad_blue" || b.action=="pad_green")instrumentAction=true;
        else if(p.instrument=="drums_pro")instrumentAction=b.action=="cymbal_yellow" || b.action=="cymbal_blue" || b.action=="cymbal_green";
        else if(p.instrument=="drums_5lane")instrumentAction=b.action=="pad_orange";
        if(!common && !(guitar && guitarCommon) && !instrumentAction)return fail("Action does not belong to this instrument");
        for(const auto& old:p.bindings)if(old.action==b.action)return fail("Duplicate logical action");
        if(auto alternatives=entry.find("alternatives");alternatives!=entry.end()) {
            if(p.schema<2 || !alternatives->is_array() || alternatives->size()>4)return fail("Invalid alternative inputs");
            for(const auto& item:*alternatives){Locator input;if(!locator(item,p.source,p.reportLength,input))return fail("Invalid alternative input");b.alternatives.push_back(std::move(input));}
        }
        auto v=entry.find("velocity");if(v!=entry.end()){if(guitar || common || !locator(*v,p.source,p.reportLength,b.velocity)||b.velocity.kind!=Kind::Analog)return fail("Invalid velocity calibration");b.hasVelocity=true;}
        p.bindings.push_back(std::move(b));
    }
    out=std::move(p);return true;
}
std::string writeProfile(const ControllerProfile& p){
    if(!validUtf8(p.name)||!validUtf8(p.instrument))return {};
    for(const auto& b:p.bindings)if(!validUtf8(b.action)||!validUtf8(b.input.padButton)||!validUtf8(b.input.padAxis)||!validUtf8(b.velocity.padButton)||!validUtf8(b.velocity.padAxis))return {};
    for(const auto& b:p.menuBindings)if(!validUtf8(b.action))return {};
    Json j={{"schema",p.schema},{"device",{{"vid",hexString(p.vid)},{"pid",hexString(p.pid)},{"name",p.name},{"source",p.source==Source::Native?"native":"raw_hid"}}},{"instrument",p.instrument},{"bindings",Json::array()}};
    if(p.source==Source::RawHid) {
        j["report"]={{"length",p.reportLength}};
        if(p.reportId>=0)j["report"]["id"]=p.reportId;
        Json menu=Json::object();if(p.menuX>=0)menu["x"]=p.menuX;if(p.menuY>=0)menu["y"]=p.menuY;
        if(p.menuHat>=0){menu["hat"]=p.menuHat;menu["hat_mask"]=hexString(p.menuHatMask);}
        if(p.suppressHat)menu["hat_ignore_when"]=serialize(p.hatSuppression,p.source);
        if(!p.menuBindings.empty()) {
            menu["buttons"]=Json::array();
            for(const auto& binding:p.menuBindings) {
                auto entry=serialize(binding.input,p.source);entry["action"]=binding.action;
                if(!binding.alternatives.empty()){entry["alternatives"]=Json::array();for(const auto& input:binding.alternatives)entry["alternatives"].push_back(serialize(input,p.source));}
                menu["buttons"].push_back(std::move(entry));
            }
        }
        if(!menu.empty())j["menu"]=std::move(menu);
    }
    for(const auto& b:p.bindings){auto v=serialize(b.input,p.source);v["action"]=b.action;if(!b.alternatives.empty()){v["alternatives"]=Json::array();for(const auto& input:b.alternatives)v["alternatives"].push_back(serialize(input,p.source));}if(b.hasVelocity)v["velocity"]=serialize(b.velocity,p.source);j["bindings"].push_back(std::move(v));}
    return j.dump(2)+'\n';
}
bool loadProfile(const char* path,ControllerProfile& out,std::string& error){
    FILE* f=std::fopen(path,"rb");if(!f){error="Could not open controller profile";return false;}std::string data;char b[4096];size_t n;
    while((n=std::fread(b,1,sizeof(b),f))){data.append(b,n);if(data.size()>65536)break;}
    const bool ok=!std::ferror(f);std::fclose(f);if(!ok){error="Could not read controller profile";return false;}return parseProfile(data,out,error);
}
bool saveProfile(const char* path,const ControllerProfile& profile,std::string& error){
    const std::string data=writeProfile(profile);ControllerProfile checked;if(!parseProfile(data,checked,error))return false;
    const std::string tmp=std::string(path)+".tmp";FILE* f=std::fopen(tmp.c_str(),"wb");if(!f){error="Could not create controller profile";return false;}
    bool ok=std::fwrite(data.data(),1,data.size(),f)==data.size();if(ok)ok=commitFile(f);if(std::fclose(f))ok=false;
    if(ok && std::rename(tmp.c_str(),path)==0)return true;std::remove(tmp.c_str());error="Could not commit controller profile";return false;
}
InstrumentState applyProfile(const ControllerProfile& p,const NativeSample& native,const uint8_t* raw,unsigned size) noexcept {
    InstrumentState out;if(p.source==Source::RawHid && (size!=p.reportLength || (p.reportId>=0 && (!raw || !size || raw[0]!=p.reportId))))return out;
    for(const auto& b:p.bindings){float value=sample(b.input,p.source,native,raw,size);for(const auto& input:b.alternatives)value=std::max(value,sample(input,p.source,native,raw,size));const bool down=value>.5f;
        int fret=-1,pad=-1;
        for(unsigned i=0;i<5;++i)if(b.action==actions5[i])fret=int(i);
        for(unsigned i=0;i<6;++i)if(b.action==actions6[i])fret=int(i);
        if(fret>=0 && down)out.frets|=uint16_t(1u<<fret);
        constexpr const char* padActions[]{"kick","pad_red","pad_yellow","pad_blue","pad_green","cymbal_yellow","cymbal_blue","cymbal_green","pad_orange"};
        for(unsigned i=0;i<9;++i)if(b.action==padActions[i])pad=int(i);
        if(pad>=0 && down){out.pads|=uint16_t(1u<<pad);if(unsigned(pad)<out.velocities.size() && b.hasVelocity){out.velocities[pad]=sample(b.velocity,p.source,native,raw,size);out.velocityKnown|=uint16_t(1u<<pad);}}
        if(b.action=="strum_up")out.strumUp=down;else if(b.action=="strum_down")out.strumDown=down;
        else if(b.action=="start")out.start=down;else if(b.action=="star_power")out.starPower=down;
        else if(b.action=="whammy")out.whammy=value;else if(b.action=="tilt")out.tilt=value;
    }return out;
}
bool sameControllerInput(const ControllerProfile& profile,const uint8_t* a,unsigned aSize,const uint8_t* b,unsigned bSize) noexcept {
    return aSize==bSize && applyProfile(profile,{},a,aSize)==applyProfile(profile,{},b,bSize) &&
        rawMenuControls(profile,a,aSize)==rawMenuControls(profile,b,bSize);
}
ControllerProfile nativeProfile(int type,const Mapping& m,uint16_t vid,uint16_t pid){
    ControllerProfile p;p.name="Mapped controller";p.vid=vid;p.pid=pid;p.instrument=instrumentName(type);
    for(unsigned i=0;i<count(type);++i){Binding b;b.action=actionName(type,i);for(unsigned k=0;k<buttons.size();++k)if(buttons[k]==m.keys[i])b.input.padButton=buttonNames[k];p.bindings.push_back(std::move(b));}return p;
}
bool nativeMapping(const ControllerProfile& p,Mapping& out,int& type) noexcept {
    if(p.source!=Source::Native)return false;for(type=0;type<5;++type)if(p.instrument==instrumentName(type))break;if(type==5)return false;
    Mapping m;for(unsigned i=0;i<count(type);++i){bool found=false;for(const auto& b:p.bindings)if(b.action==actionName(type,i) && b.input.kind==Kind::Digital){m.keys[i]=nativeButton(b.input.padButton);found=true;break;}if(!found)return false;}
    if(!valid(m,type))return false;m.custom=true;out=m;return true;
}
}

#include "profile_menu.hpp"
namespace rhythm {
// Player metadata and its portable controller overrides commit as one atomic file.
// Each controllers[] item is the same schema used by a standalone device JSON.
bool loadPlayerProfiles(const char* path,std::vector<MenuProfile>& out){
    FILE* f=std::fopen(path,"rb");if(!f)return false;std::string data;char buffer[4096];size_t n;
    while((n=std::fread(buffer,1,sizeof(buffer),f))){data.append(buffer,n);if(data.size()>1024*1024)break;}
    bool ok=!std::ferror(f);std::fclose(f);if(!ok||data.size()>1024*1024)return false;
    const auto root=controls::Json::parse(data,nullptr,false);
    if(root.is_discarded()||!root.is_object()||root.find("schema")==root.end()||!root["schema"].is_number_integer()||root["schema"].get<int64_t>()!=1||root.find("players")==root.end()||!root["players"].is_array()||root["players"].size()>32)return false;
    std::vector<MenuProfile> players;
    for(const auto& j:root["players"]){
        MenuProfile p;int controller,length;float speed;
        if(!j.is_object()||!controls::text(j,"name",p.name)||p.name.empty()||p.name.size()>24||p.name.find_first_of("\r\n")!=std::string::npos||!controls::number(j,"speed",speed)||speed<.5f||speed>2||!controls::integer(j,"controller",0,4,controller)||!controls::integer(j,"highway_length",20,1000,length)||length%10)return false;
        for(const auto& other:players)if(other.name==p.name)return false;
        p.speed=speed;p.controller=controller;p.highwayLength=length;
        if(const auto offsets=j.find("input_offsets_ms");offsets!=j.end()) {
            if(!offsets->is_array() || offsets->size()!=p.inputOffsetsMs.size())return false;
            for(size_t type=0;type<p.inputOffsetsMs.size();++type){
                if(!(*offsets)[type].is_number())return false;
                const auto value=(*offsets)[type].get<double>();
                if(!std::isfinite(value) || std::abs(value)>MaxPlayerOffsetMs)return false;
                p.inputOffsetsMs[type]=float(value);
            }
        }
        for(auto pair:{std::pair{"gamepad",&p.gamepad},{"lefty",&p.lefty},{"alternate_tap",&p.alternateTap},{"bot",&p.bot},{"show_accuracy",&p.showAccuracy},{"display_name",&p.displayName}}){
            auto i=j.find(pair.first);if(i==j.end()||!i->is_boolean())return false;*pair.second=i->get<bool>();
        }
        if(const auto effect=j.find("highway_effects");effect!=j.end()){if(!effect->is_boolean())return false;p.highwayEffects=effect->get<bool>();}
        if(j.find("note_color")!=j.end() && !controls::integer(j,"note_color",0,int(noteColorNames.size())-1,p.noteColor))return false;
        if(j.find("star_power_color")!=j.end() && !controls::integer(j,"star_power_color",0,int(starPowerColors.size())-1,p.starPowerColor))return false;
        if(j.find("highway_style")!=j.end() && !controls::integer(j,"highway_style",0,int(highwayStyles.size())-1,p.highwayStyle))return false;
        if(const auto tap=j.find("purple_tap_notes");tap!=j.end()) {
            if(!tap->is_boolean())return false;p.purpleTapNotes=tap->get<bool>();
        }
        if(const auto effects=j.find("lane_hit_effects");effects!=j.end()) {
            if(!effects->is_boolean())return false;p.laneHitEffects=effects->get<bool>();
        }
        if(const auto explicitColor=j.find("kick_color_explicit");explicitColor!=j.end()){if(!explicitColor->is_boolean())return false;p.kickColorExplicit=explicitColor->get<bool>();}
        if(const auto kick=j.find("purple_kick");kick!=j.end()) {
            if(!kick->is_boolean())return false;p.purpleKick=kick->get<bool>();
        }
        auto list=j.find("controllers");if(list==j.end()||!list->is_array()||(list->size()<4 || list->size()>64))return false;
        unsigned seen=0;
        for(const auto& item:*list){controls::ControllerProfile portable;std::string error;controls::Mapping mapping;int type;
            if(!controls::parseProfile(item.dump(),portable,error))return false;
            for(type=0;type<5;++type)if(portable.instrument==controls::instrumentName(type))break;
            if(type==5)return false;
            if(portable.source==controls::Source::Native) {
                if((seen&(1u<<type)) || !controls::nativeMapping(portable,mapping,type))return false;
                auto custom=item.find("custom");if(custom==item.end()||!custom->is_boolean())return false;
                mapping.custom=custom->get<bool>();p.mappings[type]=mapping;
                seen|=1u<<type;
            } else {
                if(p.rawMapping(type,portable.vid,portable.pid))return false;
                p.rawMappings.push_back(std::move(portable));
            }
        }
        players.push_back(std::move(p));
    }
    out=std::move(players);return true;
}
bool savePlayerProfiles(const char* path,const std::vector<MenuProfile>& players){
    controls::Json root={{"schema",1},{"players",controls::Json::array()}};
    if(players.size()>32)return false;
    for(const auto& p:players){
        for(const auto offset:p.inputOffsetsMs)if(!std::isfinite(offset) || std::abs(offset)>MaxPlayerOffsetMs)return false;
        if(p.name.empty() || p.name.size()>24 || !controls::validUtf8(p.name) || p.name.find_first_of("\r\n")!=std::string::npos || !std::isfinite(p.speed) || p.speed<.5f || p.speed>2 || p.controller<0 || p.controller>4 || p.highwayLength<20 || p.highwayLength>1000 || p.highwayLength%10)return false;
        controls::Json j={{"name",p.name},{"speed",p.speed},{"controller",p.controller},{"highway_length",p.highwayLength},{"gamepad",p.gamepad},{"lefty",p.lefty},{"alternate_tap",p.alternateTap},{"bot",p.bot},{"show_accuracy",p.showAccuracy},{"display_name",p.displayName},{"purple_kick",p.purpleKick},{"kick_color_explicit",p.kickColorExplicit},{"lane_hit_effects",p.laneHitEffects},{"purple_tap_notes",p.purpleTapNotes},{"highway_style",p.highwayStyle},{"star_power_color",p.starPowerColor},{"note_color",p.noteColor},{"highway_effects",p.highwayEffects},{"controllers",controls::Json::array()}};
        j["input_offsets_ms"]=p.inputOffsetsMs;
        for(int type=0;type<5;++type){if(!controls::valid(p.mappings[type],type))return false;
            auto item=controls::Json::parse(controls::writeProfile(controls::nativeProfile(type,p.mappings[type])),nullptr,false);
            if(item.is_discarded())return false;item["custom"]=p.mappings[type].custom;j["controllers"].push_back(std::move(item));
        }
        for(const auto& raw:p.rawMappings) {
            auto item=controls::Json::parse(controls::writeProfile(raw),nullptr,false);
            controls::ControllerProfile checked;std::string error;
            if(!controls::parseProfile(item.dump(),checked,error))return false;
            item["custom"]=true;j["controllers"].push_back(std::move(item));
        }
        root["players"].push_back(std::move(j));
    }
    const auto data=root.dump(2)+'\n';if(data.size()>1024*1024)return false;
    const std::string temporary=std::string(path)+".tmp";
    FILE* file=std::fopen(temporary.c_str(),"wb");if(!file)return false;
    bool ok=std::fwrite(data.data(),1,data.size(),file)==data.size();if(ok)ok=controls::commitFile(file);if(std::fclose(file))ok=false;
    if(ok&&std::rename(temporary.c_str(),path)==0)return true;std::remove(temporary.c_str());return false;
}
}
namespace rhythm::controls {
void ReportCapture::begin(const uint8_t* report,unsigned size) noexcept {
    length=std::min(64u,size);std::copy_n(report,length,baseline.begin());previous=baseline;
}
bool ReportCapture::capture(const uint8_t* report,unsigned size,bool hat,Locator& out,std::string& error,const ControllerProfile* known){
    if(!report || !length || size!=length)return false;
    int byte=-1;uint8_t mask=0;uint32_t capturedHatMask=0;unsigned changes=0;
    for(unsigned i=0;i<size;++i){
        uint32_t permitted=known?0:255;
        if(known)for(const auto& binding:known->bindings) {
            const auto allow=[&](const Locator& input) {
                if(unsigned(input.byte)!=i)return;
                if(hat && input.kind==Kind::Hat)permitted|=input.mask?input.mask:255;
                if(!hat && input.kind==Kind::Digital)permitted|=input.mask;
            };
            allow(binding.input);for(const auto& input:binding.alternatives)allow(input);
        }
        if(!permitted)continue;
        const auto current=report[i]&permitted,old=previous[i]&permitted,rest=baseline[i]&permitted;
        const unsigned difference=hat?(current!=old && current!=rest):(current&~old);
        if(!difference)continue;
        ++changes;byte=int(i);mask=uint8_t(difference);capturedHatMask=permitted==255?0:permitted;
    }
    previous.fill(0);std::copy_n(report,size,previous.begin());
    if(changes==0)return false;
    if(changes>1 || (!hat && (mask&(mask-1)))){error="More than one input changed. Release all inputs and try again.";return false;}
    out={};out.kind=hat?Kind::Hat:Kind::Digital;out.byte=byte;out.mask=hat?capturedHatMask:mask;out.value=hat?(report[byte]&(capturedHatMask?capturedHatMask:255)):report[byte];error.clear();return true;
}
bool sameLocator(const Locator& a,const Locator& b) noexcept {
    return a.kind==b.kind && a.byte==b.byte && (a.kind==Kind::Hat?a.value==b.value:a.mask==b.mask);
}
}

namespace rhythm::controls {
bool CapturePressFilter::accept(const Locator& input,double now,double interval) noexcept {
    if(!std::isfinite(now))return false;
    for(unsigned i=0;i<count;++i)if(sameLocator(input,inputs[i])) {
        if(now>=acceptedAt[i] && now-acceptedAt[i]<interval)return false;
        acceptedAt[i]=now;return true;
    }
    const unsigned slot=count<inputs.size()?count++:next++%inputs.size();
    inputs[slot]=input;acceptedAt[slot]=now;return true;
}
}
