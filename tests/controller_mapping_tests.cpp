#include "../examples/cloneheronative/gameplay/controller_profile.hpp"
#include <cassert>
#include <cstdio>
#include <unistd.h>
#include "../examples/cloneheronative/gameplay/profile_menu.hpp"
using namespace rhythm::controls;
int main(){
 for(int type=0;type<5;++type){
  auto m=defaults(type);assert(valid(m,type));Remap r;r.begin(type,m);
  while(r.page==Remap::Page::Capture)r.capture(m.keys[r.index]);
  assert(r.page==Remap::Page::Review && r.draft.keys==m.keys);
  r.begin(type,m);r.capture(m.keys[1]);assert(r.page==Remap::Page::Conflict);r.retryConflict();assert(r.draft.keys==m.keys);
  r.capture(m.keys[1]);r.swap();assert(r.index==1 && valid(r.draft,type) && r.draft.keys[1]==m.keys[0]);
  ControllerProfile p;std::string error;assert(parseProfile(writeProfile(nativeProfile(type,m)),p,error));
  Mapping decoded;int decodedType;assert(nativeMapping(p,decoded,decodedType)&&decoded.keys==m.keys&&decodedType==type);
  auto state=applyProfile(p,{m.keys[0],{}});assert(type<2?state.frets==1:state.pads==1);
 }
 ControllerProfile raw;std::string error;
 const std::string json=R"({"schema":1,"device":{"vid":"0x12ba","pid":"0x0100","name":"Verified guitar","source":"raw_hid"},"instrument":"guitar_5fret","report":{"length":27},"bindings":[{"action":"green","type":"digital","byte":0,"mask":"0x02"},{"action":"yellow","type":"digital","byte":0,"mask":"0x01"},{"action":"strum_up","type":"hat","byte":2,"value":0}]})";
 assert(parseProfile(json,raw,error));uint8_t report[27]{};report[0]=3;assert(applyProfile(raw,{},report,27).frets==5);assert(applyProfile(raw,{},report,27).strumUp);report[2]=1;assert(applyProfile(raw,{},report,27).strumUp);report[2]=8;assert(!applyProfile(raw,{},report,27).strumUp);assert(applyProfile(raw,{},report,26).frets==0);
 auto invalid=json;invalid.replace(invalid.find("\"schema\":1"),10,"\"schema\":4");assert(!parseProfile(invalid,raw,error));
 invalid=json;invalid.replace(invalid.find("\"byte\":0"),8,"\"byte\":99");assert(!parseProfile(invalid,raw,error));
 assert(!parseProfile("{}",raw,error));assert(!parseProfile("[true]",raw,error));assert(!parseProfile("{",raw,error));
 char dir[]="/tmp/ch-controls-XXXXXX";assert(mkdtemp(dir));std::string path=std::string(dir)+"/controller.json";
 // Raw capture follows changed inputs, not release edges; hats store values.
 uint8_t neutral[27]{};neutral[2]=8;ReportCapture capture;capture.begin(neutral,27);Locator locator;
 uint8_t changed[27]{};std::copy_n(neutral,27,changed);changed[0]=2;
 assert(capture.capture(changed,27,false,locator,error)&&locator.byte==0&&locator.mask==2);
 assert(!capture.capture(neutral,27,false,locator,error));
 changed[0]=0;changed[2]=0;assert(capture.capture(changed,27,true,locator,error)&&locator.kind==Kind::Hat&&locator.value==0);
 // Player metadata commits the same complete portable JSON profiles atomically,
 // preserving multiple devices and per-instrument native overrides.
 rhythm::ProfileMenu menu;menu.saved.push_back({"Player A"});menu.saved.push_back({"Player B"});
 ControllerProfile guitar;assert(parseProfile(json,guitar,error));menu.saved[0].setRawMapping(guitar);
 guitar.pid=0x1001;menu.saved[0].setRawMapping(guitar);
 auto custom=defaults(0);std::swap(custom.keys[0],custom.keys[1]);custom.custom=true;menu.saved[1].mappings[0]=custom;
 auto store=std::string(dir)+"/players.json";assert(menu.save(store.c_str()));rhythm::ProfileMenu restored;assert(restored.load(store.c_str()));
 assert(restored.saved[0].rawMappings.size()==2 && restored.saved[1].rawMappings.empty());
 assert(restored.saved[1].mappings[0].keys==custom.keys && restored.saved[0].mappings[0].keys!=custom.keys);
 std::remove(store.c_str());
 auto profile=nativeProfile(0,defaults(0));assert(saveProfile(path.c_str(),profile,error));assert(loadProfile(path.c_str(),raw,error));
 auto invalidUtf8=profile;invalidUtf8.name=std::string(1,char(255));assert(!saveProfile(path.c_str(),invalidUtf8,error));assert(loadProfile(path.c_str(),raw,error));
 std::remove(path.c_str());rmdir(dir);
 puts("PASS all instrument layouts, guided conflict/review staging, JSON round trips, raw/hat decoding, malformed profiles and atomic persistence");
}
