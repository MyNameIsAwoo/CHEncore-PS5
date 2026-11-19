#pragma once

#include "controller_profile.hpp"
#include "feature_availability.hpp"
#include "highway_styles.hpp"
#include "player_colors.hpp"
#include "player_calibration.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace rhythm {

// PlayerProfileMenu owns one independently navigated card per input player.
// Recovered entry/close/drop-out methods: 180178310/180174400/180178A20.
struct MenuProfile {
    std::string name;
    float speed{1.5f};
    bool gamepad{false};
    int highwayLength{100};
    // alternateTap remains readable for legacy profile files; the menu now
    // exposes a single Tap Note Color option and uses the standard source cap.
    bool lefty{false}, alternateTap{false}, bot{false}, showAccuracy{false}, displayName{false};
    // Stored IDs stay stable; new profiles default to 5-fret guitar.
    int controller{0};
    bool purpleKick{false};
    bool kickColorExplicit{false};
    bool usesPurpleKick() const noexcept {return purpleKick || (controller==3 && !kickColorExplicit);}
    bool laneHitEffects{false};
    bool purpleTapNotes{false};
    int highwayStyle{0};
    int starPowerColor{0};
    int noteColor{0};
    bool highwayEffects{false};
    std::array<controls::Mapping,5> mappings{controls::defaults(0),controls::defaults(1),controls::defaults(2),controls::defaults(3),controls::defaults(4)};
    std::vector<controls::ControllerProfile> rawMappings;
    std::array<float,5> inputOffsetsMs{};
    const controls::ControllerProfile* rawMapping(int type,uint16_t vid,uint16_t pid) const {
        for(const auto& p:rawMappings)if(p.vid==vid && p.pid==pid && p.instrument==controls::instrumentName(type))return &p;
        return nullptr;
    }
    void setRawMapping(const controls::ControllerProfile& mapping) {
        for(auto& p:rawMappings)if(p.vid==mapping.vid && p.pid==mapping.pid && p.instrument==mapping.instrument){p=mapping;return;}
        rawMappings.push_back(mapping);
    }
    static constexpr const char* controllerNames[]{"5-Fret Guitar","6-Fret Guitar","Drums","5-Lane Drums","Pro Drums"};
};

bool loadPlayerProfiles(const char*,std::vector<MenuProfile>&);
bool savePlayerProfiles(const char*,const std::vector<MenuProfile>&);

class ProfileMenu {
public:
    enum class Page { Closed, Profiles, Options, Name, Delete, Colors, Calibration };
    struct Slot {
        Page page{Page::Closed};
        int profile{-2}; // -2: unassigned, -1: transient guest, otherwise saved index
        int row{0}, key{0};
        MenuProfile guest{"Guest"};
        std::string name, message;
        bool rename{false},remapRequested{false};
        bool timingRequested{},velocityRequested{},midiAvailable{};
        float slide{0.0f};
        float slideElapsed{0.0f};
        bool expanding{false};
    };
    std::array<Slot,4> slots{};
    std::vector<MenuProfile> saved;
    bool dirty{false};

    bool load(const char* path) {
        FILE* file=std::fopen(path,"rb");
        if(!file) return false;
        char line[128];
        if(!std::fgets(line,sizeof(line),file)) {std::fclose(file);return false;}
        if(line[0]=='{') {
            std::fclose(file);std::vector<MenuProfile> loaded;
            if(!loadPlayerProfiles(path,loaded))return false;
            saved=std::move(loaded);dirty=false;return true;
        }
        const bool legacy=std::string(line)=="CHPS5PROFILES 1\n";
        const bool current=std::string(line)=="CHPS5PROFILES 4\n";
        const bool version3=std::string(line)=="CHPS5PROFILES 3\n";
        if(!legacy && !current && !version3 && std::string(line)!="CHPS5PROFILES 2\n") {
            std::fclose(file); return false;
        }
        std::vector<MenuProfile> loaded;
        bool ok=true;
        while(std::fgets(line,sizeof(line),file)) {
            char* speedEnd=nullptr;
            const float speed=std::strtof(line,&speedEnd);
            if(speedEnd==line || *speedEnd!=' ') {ok=false;break;}
            char* gamepadEnd=nullptr;
            const long gamepad=std::strtol(speedEnd+1,&gamepadEnd,10);
            if(gamepadEnd==speedEnd+1 || *gamepadEnd!=' ') {ok=false;break;}
            int length=100; unsigned long flags=0;
            char* nameStart=gamepadEnd+1;
            if(!legacy) {
                char* end=nullptr;
                const long value=std::strtol(nameStart,&end,10);
                if(end==nameStart || *end!=' ' || value<20 || value>1000 || value%10!=0) {ok=false;break;}
                length=static_cast<int>(value); nameStart=end+1;
                flags=std::strtoul(nameStart,&end,10);
                if(end==nameStart || *end!=' ' || flags>31) {ok=false;break;}
                nameStart=end+1;
            }
            int controller=0;
            if(current || version3) {
                char* end=nullptr;
                const long value=std::strtol(nameStart,&end,10);
                if(end==nameStart || *end!=' ' || value<0 || value>3) {ok=false;break;}
                controller=static_cast<int>(value); nameStart=end+1;
            }
            std::string name=nameStart;
            if(!name.empty() && name.back()=='\n') name.pop_back();
            if(!name.empty() && name.back()=='\r') name.pop_back();
            if(loaded.size()>=32 || name.empty() || name.size()>24 ||
                !std::isfinite(speed) || speed<0.5f || speed>2.0f || (gamepad!=0 && gamepad!=1)) {ok=false;break;}
            for(const auto& p:loaded) if(p.name==name) ok=false;
            if(!ok) break;
            loaded.push_back({name,speed,gamepad!=0,length,(flags&1)!=0,(flags&2)!=0,
                (flags&4)!=0,(flags&8)!=0,(flags&16)!=0,controller});
            if(current) for(int type=0;type<4;++type) {
                controls::Mapping mapping;unsigned storedType=99,custom=0;
                if(!std::fgets(line,sizeof(line),file) || std::sscanf(line,"MAP %u %u %u %u %u %u %u %u %u %u %u %u",
                    &storedType,&custom,&mapping.keys[0],&mapping.keys[1],&mapping.keys[2],&mapping.keys[3],
                    &mapping.keys[4],&mapping.keys[5],&mapping.keys[6],&mapping.keys[7],&mapping.keys[8],&mapping.keys[9])!=12 || storedType!=unsigned(type) || custom>1 ||
                    !controls::valid(mapping,type)) {ok=false;break;}
                mapping.custom=custom!=0;loaded.back().mappings[type]=mapping;
            }
            if(!ok)break;
        }
        ok=ok && !std::ferror(file);
        if(std::fclose(file)!=0) ok=false;
        if(ok) {saved=std::move(loaded);dirty=!current;}
        return ok;
    }
    bool save(const char* path) {
        if(!savePlayerProfiles(path,saved))return false;
        dirty=false;return true;
    }

    // Keep the entered name and prior assignments intact if storage rejects a
    // create/rename. Keyboard confirmation must never look like a successful save.
    template<typename Save> bool submitName(int player,Save saveChanges) {
        const auto oldSaved=saved;
        const auto oldSlot=slots[player];
        const bool oldDirty=dirty;
        commitName(player);
        if(slots[player].page==Page::Name) return false; // validation message
        if(saveChanges()) return true;
        saved=oldSaved; slots[player]=oldSlot; dirty=oldDirty;
        slots[player].message="Could not save. Please try again.";
        return false;
    }

    MenuProfile* profile(int player) {
        auto& s=slots[player];
        if(s.profile==-1) return &s.guest;
        if(s.profile>=0 && static_cast<size_t>(s.profile)<saved.size()) return &saved[s.profile];
        return nullptr;
    }
    const MenuProfile* profile(int player) const {
        const auto& s=slots[player];
        if(s.profile==-1)return &s.guest;
        if(s.profile>=0 && static_cast<size_t>(s.profile)<saved.size())return &saved[s.profile];
        return nullptr;
    }
    bool joined(int player) const { return slots[player].profile!=-2; }
    bool open(int player) const { return slots[player].page!=Page::Closed; }
    std::vector<std::string> rows(int player) const {
        const auto& s=slots[player];
        if(s.page==Page::Profiles) {
            std::vector<std::string> r;
            for(const auto& p:saved) r.push_back(p.name);
            r.push_back("Guest"); r.push_back("Create Profile"); return r;
        }
        if(s.page==Page::Delete) return {"Cancel", "Delete Profile"};
        if(s.page==Page::Calibration) {
            std::vector<std::string> r{"Timing Test","Input Offset","Reset Timing"};
            if(s.midiAvailable)r.push_back("MIDI Velocity");
            r.push_back("Back");return r;
        }
        if(s.page==Page::Colors) {
            std::vector<std::string> colors{"Hit Effect Color","Star Power Color","Highway Effects"};
            if(const auto* p=profile(player);p && p->controller<2){colors.push_back("Note Color");colors.push_back("Tap Note Color");}
            if(const auto* p=profile(player);p && p->controller>=2)colors.push_back("Kick Color");
            return colors;
        }
        std::vector<std::string> r={"Controller","Remap Controller","Calibrate Controller","Note Speed","Highway Length","Lefty Flip",
            "Gamepad Mode","Bot","Show Accuracy","Display Name","Color Options","Drop Out"};
        r.insert(r.end()-1,"Highway Style");
        if(s.profile>=0) { r.insert(r.begin(),"Delete Profile"); r.insert(r.begin(),"Rename Profile"); }
        return r;
    }
    bool enabled(const std::string& row) const {
        if(row=="Calibrate Controller")return true;
        return row=="Highway Effects" || row=="Note Color" || row=="Color Options" || row=="Star Power Color" || row=="Highway Style" || row=="Tap Note Color" || row=="Hit Effect Color" || row=="Kick Color" || row=="Remap Controller" || row=="Controller" || row=="Note Speed" || row=="Highway Length" || row=="Lefty Flip" ||
            row=="Gamepad Mode" || row=="Bot" ||
            row=="Show Accuracy" || row=="Display Name" || row=="Rename Profile" ||
            row=="Delete Profile" || row=="Drop Out";
    }
    void start(int player) {
        auto& s=slots[player];
        if(open(player)) { back(player); return; }
        s.page=joined(player)?Page::Options:Page::Profiles;
        s.row=0; s.message.clear();
    }
    void move(int player,int direction) {
        auto& s=slots[player]; const int count=static_cast<int>(rows(player).size());
        s.row=(s.row+direction+count)%count; s.message.clear();
    }
    void back(int player) {
        auto& s=slots[player]; s.message.clear();
        if(s.page==Page::Colors || s.page==Page::Calibration) {
            const bool calibration=s.page==Page::Calibration;
            s.page=Page::Options;const auto options=rows(player);
            s.row=int(std::find(options.begin(),options.end(),calibration?"Calibrate Controller":"Color Options")-options.begin());
        } else if(s.page==Page::Delete || (s.page==Page::Name && s.rename)) {
            s.page=Page::Options; s.row=0;
        } else if(s.page==Page::Name) { s.page=Page::Profiles; s.row=0; }
        else s.page=Page::Closed; // closing never drops an assigned profile
    }
    void adjust(int player,int direction) {
        auto* p=profile(player); auto& s=slots[player];
        if(!p || (s.page!=Page::Options && s.page!=Page::Colors && s.page!=Page::Calibration)) return;
        const auto row=rows(player)[s.row];
        if(s.page==Page::Calibration) {
            if(row!="Input Offset")return;
            auto& offset=p->inputOffsetsMs[std::clamp(p->controller,0,4)];
            offset=std::clamp(offset+(direction<0?-1.0f:1.0f),-MaxPlayerOffsetMs,MaxPlayerOffsetMs);
            dirty|=s.profile>=0;return;
        }
        if(row=="Controller") {
            // Keep saved IDs stable; controller order is independent of stored IDs.
            static constexpr int controllerOrder[]{0,2,4,3,1};
            int position=0;
            while(position<5 && controllerOrder[position]!=p->controller) ++position;
            if(position==5) position=0;
            const int step=direction<0?-1:1;
            do {position=(position+step+5)%5;}while(!controls::controllerTypeEnabled(controllerOrder[position]));
            p->controller=controllerOrder[position];
        }
        else if(row=="Highway Style") p->highwayStyle=(p->highwayStyle+(direction<0?-1:1)+int(highwayStyles.size()))%int(highwayStyles.size());
        else if(row=="Highway Effects") p->highwayEffects=!p->highwayEffects;
        else if(row=="Note Color") p->noteColor=(p->noteColor+(direction<0?-1:1)+int(noteColorNames.size()))%int(noteColorNames.size());
        else if(row=="Star Power Color") p->starPowerColor=(p->starPowerColor+(direction<0?-1:1)+int(starPowerColors.size()))%int(starPowerColors.size());
        else if(row=="Tap Note Color") p->purpleTapNotes=!p->purpleTapNotes;
        else if(row=="Hit Effect Color") p->laneHitEffects=!p->laneHitEffects;
        else if(row=="Kick Color") {p->purpleKick=!p->usesPurpleKick();p->kickColorExplicit=true;}
        else if(row=="Note Speed") p->speed=std::clamp(p->speed+direction*0.1f,0.5f,2.0f);
        else if(row=="Highway Length") p->highwayLength=std::clamp(p->highwayLength+direction*10,20,1000);
        else if(row=="Lefty Flip") p->lefty=!p->lefty;
        else if(row=="Gamepad Mode") p->gamepad=!p->gamepad;
        else if(row=="Bot") p->bot=!p->bot;
        else if(row=="Show Accuracy") p->showAccuracy=!p->showAccuracy;
        else if(row=="Display Name") p->displayName=!p->displayName;
        else return;
        dirty=dirty || s.profile>=0;
    }
    void accept(int player) {
        auto& s=slots[player]; s.message.clear();
        if(s.page==Page::Profiles) {
            if(s.row==static_cast<int>(saved.size())+1) {
                if(saved.size()>=32) { s.message="Profile limit reached"; return; }
                s.rename=false; s.name.clear(); s.key=0; s.page=Page::Name; return;
            }
            if(s.row<static_cast<int>(saved.size())) {
                for(int other=0;other<4;++other)
                    if(other!=player && slots[other].profile==s.row) {
                        s.message="Profile already in use"; return;
                    }
                s.profile=s.row;
            } else s.profile=-1;
            s.page=Page::Closed; s.row=0; return;
        }
        if(s.page==Page::Delete) {
            if(s.row==1 && s.profile>=0) {
                const int removed=s.profile;
                saved.erase(saved.begin()+removed);
                for(auto& slot:slots) {
                    if(slot.profile==removed) {slot.profile=-2; slot.page=Page::Profiles; slot.row=0;}
                    else if(slot.profile>removed) --slot.profile;
                    if(slot.page==Page::Profiles) slot.row=0;
                }
                dirty=true; return;
            }
            back(player); return;
        }
        if(s.page==Page::Colors) {adjust(player,1);return;}
        if(s.page==Page::Calibration) {
            const auto row=rows(player)[s.row];
            if(row=="Timing Test")s.timingRequested=true;
            else if(row=="MIDI Velocity")s.velocityRequested=true;
            else if(row=="Reset Timing") {if(auto* p=profile(player)){p->inputOffsetsMs[std::clamp(p->controller,0,4)]=0;dirty|=s.profile>=0;}}
            else if(row=="Back")back(player);
            else adjust(player,1);
            return;
        }
        if(s.page==Page::Options) {
            const auto row=rows(player)[s.row];
            if(row=="Color Options") {s.page=Page::Colors;s.row=0;return;}
            if(row=="Calibrate Controller") {s.page=Page::Calibration;s.row=0;return;}
            if(row=="Remap Controller") s.remapRequested=true;
            else if(row=="Drop Out") { s.profile=-2; s.page=Page::Closed; s.row=0; }
            else if(row=="Rename Profile") {
                s.rename=true; s.name=profile(player)->name; s.key=0; s.page=Page::Name;
            } else if(row=="Delete Profile") {s.page=Page::Delete; s.row=0;}
            else if(enabled(row)) adjust(player,1);
            else s.message="Not available in this build";
        }
    }
    void nameKey(int player) {
        auto& s=slots[player];
        if(s.key<36) {
            if(s.name.size()<24) s.name.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[s.key]);
        } else if(s.key==36) { if(s.name.size()<24) s.name.push_back(' '); }
        else if(s.key==37) { if(!s.name.empty()) s.name.pop_back(); }
        else if(s.key==38) { back(player); return; }
        else commitName(player);
    }
    void commitName(int player) {
        auto& s=slots[player];
        if(s.page!=Page::Name) return;
        {
            const size_t first=s.name.find_first_not_of(' '),last=s.name.find_last_not_of(' ');
            if(first==std::string::npos) {s.message="Enter a profile name";return;}
            const std::string name=s.name.substr(first,last-first+1);
            for(size_t i=0;i<saved.size();++i) {
                if((!s.rename || static_cast<int>(i)!=s.profile) && saved[i].name==name) {
                    s.message="Name already exists";return;
                }
            }
            if(s.rename) {
                if(!profile(player)) {s.message="Profile is no longer available";return;}
                profile(player)->name=name; s.page=Page::Options;
            } else {
                if(saved.size()>=32) {s.message="Profile limit reached";return;}
                saved.push_back({name}); s.profile=static_cast<int>(saved.size()-1);s.page=Page::Closed;
            }
            s.row=0; s.message.clear(); dirty=true;
        }
    }
};
} // namespace rhythm
