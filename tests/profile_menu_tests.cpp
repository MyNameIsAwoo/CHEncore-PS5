#include "../examples/cloneheronative/gameplay/profile_menu.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

using Menu=rhythm::ProfileMenu;
static void choose(Menu& menu,int player,const char* label) {
    auto rows=menu.rows(player);
    if(std::find(rows.begin(),rows.end(),label)==rows.end() && menu.slots[player].page==Menu::Page::Colors){menu.back(player);rows=menu.rows(player);}
    if(std::find(rows.begin(),rows.end(),label)==rows.end() && menu.slots[player].page==Menu::Page::Options){
        const auto colors=std::find(rows.begin(),rows.end(),"Color Options");assert(colors!=rows.end());
        menu.slots[player].row=int(colors-rows.begin());menu.accept(player);rows=menu.rows(player);
    }
    const auto row=std::find(rows.begin(),rows.end(),label);
    assert(row!=rows.end());
    menu.slots[player].row=static_cast<int>(row-rows.begin());
    menu.accept(player);
}

int main() {
    {
        rhythm::MenuProfile defaults{"Five lane"};defaults.controller=3;
        assert(defaults.usesPurpleKick());
        defaults.controller=2;assert(!defaults.usesPurpleKick());
        defaults.controller=3;defaults.kickColorExplicit=true;
        assert(!defaults.usesPurpleKick()); // Explicit orange survives save/load.
        char path[]="/tmp/ch-kick-default-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
        assert(rhythm::savePlayerProfiles(path,{defaults}));
        std::vector<rhythm::MenuProfile> restored;assert(rhythm::loadPlayerProfiles(path,restored));
        assert(restored.size()==1 && restored[0].kickColorExplicit && !restored[0].usesPurpleKick());
        unlink(path);
    }

    {
        char directory[]="/tmp/ch-profiles-XXXXXX";
        assert(mkdtemp(directory));
        const std::string path=std::string(directory)+"/profiles.cfg";
        const std::string missing=std::string(directory)+"/missing/profiles.cfg";
        Menu storage;
        storage.start(0); choose(storage,0,"Create Profile");
        storage.slots[0].name="  SAVED PROFILE  ";
        assert(!storage.submitName(0,[&] {return storage.save(missing.c_str());}));
        assert(storage.saved.empty() && !storage.joined(0));
        assert(storage.slots[0].page==Menu::Page::Name);
        assert(storage.slots[0].name=="  SAVED PROFILE  " && !storage.slots[0].message.empty());
        assert(storage.submitName(0,[&] {return storage.save(path.c_str());}));
        assert(storage.joined(0) && !storage.open(0) && !storage.dirty);
        Menu reloaded;
        assert(reloaded.load(path.c_str()));
        assert(reloaded.saved.size()==1 && reloaded.saved[0].name=="SAVED PROFILE");
        auto& settings=reloaded.saved[0];
        settings.highwayLength=230;settings.highwayStyle=3;settings.starPowerColor=3;settings.noteColor=9;settings.highwayEffects=true;
        settings.controller=3;
        settings.lefty=settings.alternateTap=settings.bot=settings.showAccuracy=settings.displayName=settings.purpleKick=settings.laneHitEffects=settings.purpleTapNotes=true;
        assert(reloaded.save(path.c_str()));
        Menu configured;
        assert(configured.load(path.c_str()));
        const auto& restored=configured.saved[0];
        assert(restored.controller==3 && restored.highwayStyle==3 && restored.starPowerColor==3 && restored.noteColor==9 && restored.highwayEffects);
        configured.start(1);choose(configured,1,"Guest");configured.start(1);
        assert(configured.profile(1)->highwayStyle==0);
        choose(configured,1,"Highway Style");assert(configured.profile(1)->highwayStyle==1);
        configured.adjust(1,-1);assert(configured.profile(1)->highwayStyle==0);
        configured.adjust(1,-1);assert(configured.profile(1)->highwayStyle==4);
        assert(configured.saved[0].highwayStyle==3);
        assert(restored.highwayLength==230 && restored.lefty && restored.alternateTap && restored.bot &&
            restored.showAccuracy && restored.displayName && restored.purpleKick && restored.laneHitEffects && restored.purpleTapNotes);
        reloaded.start(2); choose(reloaded,2,"SAVED PROFILE");
        assert(reloaded.profile(2)->name=="SAVED PROFILE");
        reloaded.start(2); choose(reloaded,2,"Rename Profile");
        reloaded.slots[2].name="RENAMED";
        assert(!reloaded.submitName(2,[&] {return reloaded.save(missing.c_str());}));
        assert(reloaded.saved[0].name=="SAVED PROFILE" && reloaded.slots[2].name=="RENAMED");
        assert(reloaded.submitName(2,[&] {return reloaded.save(path.c_str());}));
        Menu renamed;
        assert(renamed.load(path.c_str()) && renamed.saved[0].name=="RENAMED");
        reloaded.slots[2].page=Menu::Page::Name;
        reloaded.slots[2].name="   ";
        bool writerCalled=false;
        assert(!reloaded.submitName(2,[&] {writerCalled=true;return true;}));
        assert(!writerCalled);
        FILE* legacy=std::fopen(path.c_str(),"wb");
        assert(legacy);
        assert(std::fprintf(legacy,"CHPS5PROFILES 1\n1.00 1 LEGACY NAME\n")>0);
        assert(std::fclose(legacy)==0);
        Menu migrated;
        assert(migrated.load(path.c_str()) && migrated.dirty);
        assert(migrated.saved[0].name=="LEGACY NAME" && migrated.saved[0].gamepad);
        assert(migrated.saved[0].highwayLength==100 && !migrated.saved[0].bot);
        assert(migrated.save(path.c_str()));
        FILE* v2=std::fopen(path.c_str(),"wb");
        assert(v2);
        assert(std::fprintf(v2,"CHPS5PROFILES 2\n1.20 0 120 4 OLD BOT\n")>0);
        assert(std::fclose(v2)==0);
        Menu migratedV2;
        assert(migratedV2.load(path.c_str()) && migratedV2.dirty);
        assert(migratedV2.saved[0].controller==0 && migratedV2.saved[0].bot);
        assert(migratedV2.saved[0].name=="OLD BOT");
        assert(std::remove(path.c_str())==0);
        assert(rmdir(directory)==0);
    }
    Menu menu;
    menu.start(0);
    assert(!menu.joined(0) && menu.slots[0].page==Menu::Page::Profiles);
    choose(menu,0,"Guest");
    assert(menu.joined(0) && !menu.open(0));
    menu.start(0);
    assert(menu.slots[0].page==Menu::Page::Options);
    menu.back(0);
    assert(menu.joined(0) && !menu.open(0)); // Back preserves assignment.

    menu.start(1);
    choose(menu,1,"Create Profile");
    menu.slots[1].name="  HOWL  "; menu.slots[1].key=39; menu.nameKey(1);
    assert(menu.profile(1)->name=="HOWL" && menu.saved.size()==1 && menu.dirty);
    assert(menu.profile(0)->name=="Guest");
    menu.start(2); choose(menu,2,"HOWL");
    assert(!menu.joined(2) && !menu.slots[2].message.empty());
    choose(menu,2,"Guest");
    assert(menu.joined(2) && menu.profile(0)!=menu.profile(2));

    const float originalSpeed=menu.profile(0)->speed;
    menu.start(1); choose(menu,1,"Note Speed");
    assert(menu.profile(1)->speed>originalSpeed && menu.profile(0)->speed==originalSpeed);
    choose(menu,1,"Gamepad Mode");
    assert(menu.profile(1)->gamepad && !menu.profile(2)->gamepad);
    choose(menu,1,"Rename Profile");
    menu.slots[1].name="HOWL PS5"; menu.slots[1].key=39; menu.nameKey(1);
    assert(menu.saved[0].name=="HOWL PS5" && menu.slots[1].page==Menu::Page::Options);

    choose(menu,1,"Delete Profile");
    menu.accept(1); // Default selection is Cancel.
    assert(menu.saved.size()==1 && menu.joined(1));
    choose(menu,1,"Drop Out");
    assert(!menu.joined(1) && !menu.open(1) && menu.saved.size()==1);
    menu.start(2); choose(menu,2,"Drop Out");
    menu.start(2); choose(menu,2,"HOWL PS5");
    assert(menu.profile(2)->name=="HOWL PS5");

    menu.start(3); choose(menu,3,"Create Profile");
    menu.slots[3].name="SECOND"; menu.slots[3].key=39; menu.nameKey(3);
    menu.start(2); choose(menu,2,"Delete Profile");
    menu.slots[2].row=1; menu.accept(2);
    assert(menu.saved.size()==1 && !menu.joined(2));
    assert(menu.profile(3)->name=="SECOND"); // Erase fixes other slot indices.
    assert(menu.rows(2).size()==3 && menu.slots[2].row==0);

    menu.start(3);
    for(const auto& row:menu.rows(3)) assert(menu.enabled(row));
    for(const char* option:{"Lefty Flip","Bot","Show Accuracy","Display Name"})
        choose(menu,3,option);
    const auto* profile=menu.profile(3);
    assert(profile->lefty && !profile->alternateTap && profile->bot && profile->showAccuracy && profile->displayName);
    assert(!menu.profile(0)->lefty && !menu.profile(0)->bot);
    // Five-fret stays first and six-fret is last; every controller remains selectable.
    choose(menu,3,"Controller");
    assert(profile->controller==2);
    choose(menu,3,"Hit Effect Color");assert(profile->laneHitEffects && !menu.profile(0)->laneHitEffects);
    choose(menu,3,"Kick Color");assert(profile->purpleKick && !menu.profile(0)->purpleKick);
    choose(menu,3,"Star Power Color");assert(profile->starPowerColor==1 && menu.profile(0)->starPowerColor==0);
    menu.back(3);assert(menu.slots[3].page==Menu::Page::Options && menu.rows(3)[menu.slots[3].row]=="Color Options");
    choose(menu,3,"Controller"); assert(profile->controller==4);
    choose(menu,3,"Controller"); assert(profile->controller==3);
    choose(menu,3,"Controller"); assert(profile->controller==1);
    choose(menu,3,"Controller"); assert(profile->controller==0);
    choose(menu,3,"Note Color");assert(profile->noteColor==1 && menu.profile(0)->noteColor==0);
    menu.back(3);
    const auto controllerRows=menu.rows(3);menu.slots[3].row=int(std::find(controllerRows.begin(),controllerRows.end(),"Controller")-controllerRows.begin());
    menu.adjust(3,-1); assert(profile->controller==1);
    choose(menu,3,"Highway Length");
    assert(profile->highwayLength==110);
    for(int i=0;i<150;++i) menu.adjust(3,1);
    assert(profile->highwayLength==1000);
    for(int i=0;i<150;++i) menu.adjust(3,-1);
    assert(profile->highwayLength==20);
    for(const char* omitted:{"Tilt","Color Profile","Highway","Options","Alternate Tap Texture"}) {
        const auto rows=menu.rows(3);
        assert(std::find(rows.begin(),rows.end(),omitted)==rows.end());
    }
    puts("Profile create/rename persistence, save failure recovery, assignment, guest isolation, delete and dropout checks passed");
}
