// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#include "song_package.hpp"
#include "../gameplay/case_file.hpp"
#include "../gameplay/pc_chart_rules.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>
namespace chorus {
namespace {
struct MidiNote {int64_t start{},end{};unsigned pitch{},velocity{};};
struct Text {int64_t tick;std::string value;};
struct Track {std::string name;std::vector<MidiNote> notes;std::vector<Text> texts;struct Phrase{int64_t start,end;unsigned type,difficulty;};std::vector<Phrase> phrases;};
struct ChartEvent {int64_t tick;std::string value;};
struct Reader {
 const uint8_t* p{};size_t size{},at{};
 bool byte(uint8_t& out){if(at>=size)return false;out=p[at++];return true;}
 bool vlq(uint32_t& out){out=0;for(int i=0;i<4;++i){uint8_t b;if(!byte(b))return false;out=(out<<7)|(b&127);if(!(b&128))return true;}return false;}
 bool skip(size_t n){if(n>size-at)return false;at+=n;return true;}
};
uint32_t be32(const uint8_t* p){return(uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
uint16_t be16(const uint8_t* p){return static_cast<uint16_t>((p[0]<<8)|p[1]);}
std::string upper(std::string s){for(char& c:s)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));return s;}
std::string quoteText(std::string s){for(char& c:s)if(c=='"'||c=='\r'||c=='\n'||c=='\0')c=' ';return s;}
const char* trackSection(const std::string& n){
 if(n=="PART GUITAR GHL")return "GHLGuitar";
 if(n=="PART BASS GHL")return "GHLBass";
 if(n=="PART RHYTHM GHL")return "GHLRhythm";
 if(n=="PART GUITAR COOP GHL")return "GHLCoop";
 if(n=="PART GUITAR"||n=="T1 GEMS")return "Single";
 if(n=="PART BASS")return "DoubleBass";
 if(n=="PART RHYTHM")return "DoubleRhythm";
 if(n=="PART GUITAR COOP")return "DoubleGuitar";
 if(n=="PART DRUMS")return "Drums";
 if(n=="PART KEYS")return "Keyboard";
 return nullptr;
}
int frequencyFor(const std::string& chart,bool& eighth,int& cutoff,bool& fiveLane){
 const auto slash=chart.find_last_of('/');const std::string path=rhythm::caseFile(chart.substr(0,slash),"song.ini");
 FILE* in=std::fopen(path.c_str(),"rb");if(!in)return 0;
 int frequency=0;char line[2048];
 while(std::fgets(line,sizeof(line),in)){
  char key[96]{},value[256]{};
  if(std::sscanf(line," %95[^= ] = %255[^\r\n]",key,value)!=2)continue;
  const std::string k=upper(key),v=upper(value);
  if(k=="HOPO_FREQUENCY")frequency=std::max(0,std::atoi(value));
  if(k=="EIGHTHNOTE_HOPO")eighth=v=="TRUE"||v=="1";
  if(k=="SUSTAIN_CUTOFF_THRESHOLD")cutoff=std::atoi(value);
  if(k=="FIVE_LANE_DRUMS")fiveLane=v=="TRUE"||v=="1";
 }
 std::fclose(in);return frequency;
}
}
bool midiChartCacheCurrent(const std::string& chart) noexcept {
 FILE* in=std::fopen(chart.c_str(),"rb");if(!in)return false;
 char line[256];bool song=false,closed=false,imported=false,current=false;
 // Only inspect the bounded metadata header, never the note data on rescans.
 for(unsigned i=0;i<64 && std::fgets(line,sizeof(line),in);++i){
  char section[64]{};
  if(std::sscanf(line," [%63[^]]]",section)==1){if(song)break;song=std::strcmp(section,"Song")==0;continue;}
  if(!song)continue;
  if(std::strchr(line,'}')){closed=true;break;}
  char key[64]{};unsigned value=0;
  if(std::sscanf(line," %63[^= ] = %u",key,&value)!=2)continue;
  if(std::strcmp(key,"MidiImport")==0)imported=value==1;
  if(std::strcmp(key,"MidiImportVersion")==0)current=value==midiImportVersion;
 }
 std::fclose(in);return closed && imported && current;
}
bool midiToChart(const std::string& midi,const std::string& chart,std::string& error,const std::atomic<bool>* cancel) noexcept {
 const auto fail=[&](const char* message){error=message;return false;};
 const auto cancelled=[&]{return cancel&&cancel->load(std::memory_order_relaxed);};
 FILE* in=std::fopen(midi.c_str(),"rb");if(!in)return fail("Cannot read MIDI chart.");
 if(std::fseek(in,0,SEEK_END)!=0){std::fclose(in);return fail("Cannot seek MIDI chart.");}
 const long length=std::ftell(in);
 if(length<14||length>32*1024*1024||std::fseek(in,0,SEEK_SET)!=0){std::fclose(in);return fail("Invalid MIDI chart size.");}
 std::vector<uint8_t> bytes(static_cast<size_t>(length));const bool read=std::fread(bytes.data(),1,bytes.size(),in)==bytes.size();std::fclose(in);
 if(!read||std::memcmp(bytes.data(),"MThd",4)||be32(bytes.data()+4)<6||be32(bytes.data()+4)>bytes.size()-8)return fail("Invalid MIDI header.");
 const unsigned format=be16(bytes.data()+8),tracks=be16(bytes.data()+10),resolution=be16(bytes.data()+12);
 // PC header reader 0x1820CC760 also rejects SMPTE time division.
 if(format>1||!tracks||tracks>256||!resolution||(resolution&0x8000))return fail("Unsupported MIDI format or SMPTE timing.");
 size_t at=8+be32(bytes.data()+4),eventCount=0;
 std::vector<Track> parsed;std::vector<ChartEvent> sync,global;
 for(unsigned trackIndex=0;trackIndex<tracks;++trackIndex){
  if(cancelled())return fail("Install cancelled.");
  if(at>bytes.size()||bytes.size()-at<8||std::memcmp(bytes.data()+at,"MTrk",4))return fail("Missing MIDI track.");
  const uint32_t size=be32(bytes.data()+at+4);at+=8;if(size>bytes.size()-at)return fail("Truncated MIDI track.");
  Reader r{bytes.data()+at,size,0};at+=size;Track track;
  std::array<std::vector<std::pair<int64_t,unsigned>>,2048> held;
  std::array<std::array<int64_t,256>,2> phraseStart{};for(auto& row:phraseStart)row.fill(-1);
  uint8_t running=0;int64_t tick=0;bool ended=false;
  while(r.at<r.size){
   if(++eventCount>2000000||cancelled())return fail("MIDI event limit reached or install cancelled.");
   uint32_t delta;if(!r.vlq(delta)||tick>INT32_MAX-delta)return fail("Invalid MIDI delta time.");tick+=delta;
   uint8_t status;if(!r.byte(status))return fail("Truncated MIDI event.");
   if(status<0x80){if(!running)return fail("MIDI running status without a channel event.");--r.at;status=running;}
   else if(status<0xf0)running=status;
   // PC SMF track reader 0x1820F5070 updates running status only for
   // channel events; PS tap/open SysEx and meta events preserve it.
   // Several shipped charts depend on this permissive reader behavior.
   if(status==0xff){
    uint8_t type;uint32_t n;if(!r.byte(type)||!r.vlq(n)||n>r.size-r.at)return fail("Truncated MIDI meta event.");const uint8_t* data=r.p+r.at;
    if(type==3)track.name=upper(std::string(reinterpret_cast<const char*>(data),n));
    else if(type==0x51){if(n!=3)return fail("Invalid MIDI tempo.");const uint32_t tempo=(uint32_t(data[0])<<16)|(uint32_t(data[1])<<8)|data[2];if(!tempo)return fail("Zero MIDI tempo.");sync.push_back({tick,"B "+std::to_string(static_cast<unsigned>(60000000000ull/tempo))});sync.push_back({tick,"MPQN "+std::to_string(tempo)});}
    else if(type==0x58){if(n!=4||!data[0]||data[1]>15)return fail("Invalid MIDI time signature.");sync.push_back({tick,"TS "+std::to_string(data[0])+" "+std::to_string(data[1])});}
    else if(type==1||type==5||type==6)track.texts.push_back({tick,std::string(reinterpret_cast<const char*>(data),n)});
    if(!r.skip(n))return fail("Truncated MIDI meta body.");
    if(type==0x2f){if(n)return fail("Invalid MIDI end-of-track.");ended=true;break;}
   }else if(status==0xf0||status==0xf7){
    uint32_t n;if(!r.vlq(n)||n>r.size-r.at)return fail("Truncated MIDI SysEx.");const auto* data=r.p+r.at;
    // PC guitar parser 0x182139E60 recognizes PS phrase types 1 (open)
    // and 4 (tap), with a difficulty byte or 0xff for all difficulties.
    if(n>=7&&data[0]=='P'&&data[1]=='S'&&data[2]==0&&data[3]==0&&(data[5]==1||data[5]==4)){
     const unsigned kind=data[5]==1?0:1,diff=data[4];
     if(data[6])phraseStart[kind][diff]=tick;
     else if(phraseStart[kind][diff]>=0){track.phrases.push_back({phraseStart[kind][diff],tick,data[5],diff});phraseStart[kind][diff]=-1;}
    }
    r.skip(n);
   }else if(status>=0xf0)return fail("Unsupported MIDI system event.");
   else{
    uint8_t pitch,value=0;if(!r.byte(pitch)||pitch>127)return fail("Invalid MIDI channel byte.");
    const unsigned op=status>>4;
    if(op!=0xc&&op!=0xd)if(!r.byte(value)||value>127)return fail("Invalid MIDI channel event.");
    if(op==9||op==8){
     auto& starts=held[(status&15)*128+pitch];
     if(op==9&&value){if(starts.size()>=32)return fail("Too many overlapping MIDI notes.");starts.push_back({tick,value});}
     else if(!starts.empty()){const auto start=starts.front();starts.erase(starts.begin());track.notes.push_back({start.first,tick,pitch,start.second});}
    }
   }
  }
  if(!ended)return fail("MIDI track has no end marker.");
  for(const auto& starts:held)if(!starts.empty())return fail("MIDI chart contains an unterminated note.");
  parsed.push_back(std::move(track));
 }
 std::map<std::string,std::vector<ChartEvent>> sections;
 bool eighth=false,configuredFiveLane=false;int cutoff=-1;const int frequency=frequencyFor(midi,eighth,cutoff,configuredFiveLane);
 // MIDI config 0x182138FA0 uses floor(PPQ/3)+1; .chart uses 65/192.
 const int threshold=frequency>0?std::min(frequency,65535):eighth?static_cast<int>(resolution/2+1):static_cast<int>(resolution/3+1);
 if(cutoff<0)cutoff=static_cast<int>(resolution/3+1);
 size_t playable=0;int64_t globalLastTick=0,authoredEndTick=-1;
 // PC vocal reader 0x18213AF10 uses MIDI 105/106 phrase notes and
 // non-bracket lyric text. Keep them in Events so every instrument shares
 // one vocal timeline. Pitch/velocity do not affect instrumental scoring.
 const auto vocals=std::find_if(parsed.begin(),parsed.end(),[](const Track& t){return t.name=="PART VOCALS";});
 if(vocals!=parsed.end()) {
  std::vector<std::pair<int64_t,int64_t>> phrases;
  for(const auto& n:vocals->notes)if((n.pitch==105||n.pitch==106)&&n.end>n.start)phrases.push_back({n.start,n.end});
  std::sort(phrases.begin(),phrases.end());
  std::vector<std::pair<int64_t,int64_t>> unique;
  for(const auto& phrase:phrases) {
   if(!unique.empty()&&phrase.first<unique.back().second)unique.back().second=std::max(unique.back().second,phrase.second);
   else if(unique.empty()||phrase!=unique.back())unique.push_back(phrase);
  }
  for(const auto& phrase:unique) {
   global.push_back({phrase.first,"E \"phrase_start\""});
   for(const auto& text:vocals->texts)if(text.tick>=phrase.first&&text.tick<phrase.second&&!text.value.empty()&&text.value.front()!='[')
    global.push_back({text.tick,"E \"lyric "+quoteText(text.value)+"\""});
   global.push_back({phrase.second,"E \"phrase_end\""});
  }
 }
 for(const Track& track:parsed){
  if(track.name=="EVENTS")for(const auto& text:track.texts){
   global.push_back({text.tick,"E \""+quoteText(text.value)+"\""});
   if(text.value=="[end]" && authoredEndTick<0)authoredEndTick=text.tick;
  }
  const char* suffix=trackSection(track.name);if(!suffix)continue;const bool drums=track.name=="PART DRUMS",six=track.name.ends_with(" GHL");
  const bool enhanced=std::any_of(track.texts.begin(),track.texts.end(),[](const Text& t){return upper(t.value)=="[ENHANCED_OPENS]";});
  std::array<std::vector<std::pair<int64_t,int64_t>>,128> regions;
  for(const auto& n:track.notes)regions[n.pitch].push_back({n.start,n.end});
  for(auto& row:regions){
   std::sort(row.begin(),row.end());size_t write=0;
   for(const auto& range:row){if(write&&range.first<=row[write-1].second)row[write-1].second=std::max(row[write-1].second,range.second);else row[write++]=range;}
   row.resize(write);
  }
  const auto inPitch=[&](unsigned pitch,int64_t tick){const auto& row=regions[pitch];const auto found=std::upper_bound(row.begin(),row.end(),tick,[](int64_t t,const auto& region){return t<region.first;});return found!=row.begin()&&tick<std::prev(found)->second;};
  const bool hasSp116=std::any_of(track.notes.begin(),track.notes.end(),[](const MidiNote& n){return n.pitch==116;});
  // MIDI reader 0x1821383B0 / pitch predicate 0x1820C6630 detects
  // a GH five-lane source across every difficulty. Its blue and green are
  // pads, while yellow/orange are cymbals. Preserve the distinct N5 green.
  const bool fiveLane=drums&&(configuredFiveLane||std::any_of(track.notes.begin(),track.notes.end(),[](const MidiNote& n){return n.pitch>=65&&n.pitch<=101&&(n.pitch-65)%12==0;}));
  const size_t oldSpCount=static_cast<size_t>(std::count_if(track.notes.begin(),track.notes.end(),[](const MidiNote& n){return n.pitch==103;}));
  for(unsigned difficulty=0;difficulty<4;++difficulty){
   static constexpr const char* names[]={"Easy","Medium","Hard","Expert"};auto& out=sections[std::string(names[difficulty])+suffix];
   struct Gem{int64_t tick,length;int lane;unsigned velocity;};std::vector<Gem> gems;
   const unsigned base=60+difficulty*12;
   for(const auto& n:track.notes){
    int lane=-1;
    if(six && n.pitch>=base-2 && n.pitch<=base+4){const int index=int(n.pitch)-int(base)+1;lane=index==-1?7:index==5?8:index;}
    else if(!six && n.pitch>=base&&n.pitch<=base+4)lane=static_cast<int>(n.pitch-base);
    else if(!drums&&enhanced&&n.pitch==base-1)lane=7;
    else if(drums&&n.pitch==base+5)lane=5;
    if(lane>=0){
     if(!drums)for(const auto& phrase:track.phrases)if(phrase.type==1&&(phrase.difficulty==255||phrase.difficulty==difficulty)&&n.start>=phrase.start&&n.start<phrase.end)lane=7;
     const int64_t sustain=n.end-n.start;
     gems.push_back({n.start,drums||sustain<=cutoff?0:sustain,lane,n.velocity});
    }
   }
   if(gems.empty())continue;
   std::stable_sort(gems.begin(),gems.end(),[](const Gem& a,const Gem& b){return a.tick<b.tick;});
   // Raw drum finalizer 0x1821560A0 moves the green pad to blue when
   // a five-lane green pad/orange cymbal chord would collide after folding.
   // Normalize before deduplication, like the PC's following finalizer pass.
   if(fiveLane)for(size_t first=0;first<gems.size();) {
    size_t end=first+1;bool orange=gems[first].lane==4;
    while(end<gems.size()&&gems[end].tick==gems[first].tick)orange|=gems[end++].lane==4;
    if(orange)for(size_t i=first;i<end;++i)if(gems[i].lane==5)gems[i].lane=3;
    first=end;
   }
   // PS Open phrases can map multiple authored frets onto one Open head.
   // The PC normalizer coalesces equal tick/lane events before conversion.
   std::vector<Gem> unique;unique.reserve(gems.size());
   for(const auto& gem:gems) {
    size_t same=unique.size();
    for(size_t i=unique.size();i>0 && unique[i-1].tick==gem.tick;--i)if(unique[i-1].lane==gem.lane){same=i-1;break;}
    if(same<unique.size())unique[same].length=std::max(unique[same].length,gem.length);else unique.push_back(gem);
   }
   gems=std::move(unique);
   int64_t previous=-1;unsigned previousMask=0;
   for(size_t begin=0;begin<gems.size();){
    size_t end=begin+1;while(end<gems.size()&&gems[end].tick==gems[begin].tick)++end;
    const int64_t tick=gems[begin].tick;unsigned mask=0;
    for(size_t i=begin;i<end;++i){mask|=gems[i].lane==7?1u:2u<<(gems[i].lane==8?5:gems[i].lane);out.push_back({tick,"N "+std::to_string(gems[i].lane)+" "+std::to_string(gems[i].length)});globalLastTick=std::max(globalLastTick,tick+gems[i].length);++playable;}
    if(!drums){
     const bool natural=rhythm::naturalGuitarHopo(mask,previousMask,tick-previous,threshold);
     bool on=false,off=false,tap=false;
     on=inPitch(base+5,tick);off=inPitch(base+6,tick);tap=inPitch(104,tick);
     for(const auto& phrase:track.phrases)if(phrase.type==4&&(phrase.difficulty==255||phrase.difficulty==difficulty)&&tick>=phrase.start&&tick<phrase.end)tap=true;
     if((on&&!natural)||(off&&natural&&!on))out.push_back({tick,"N 5 0"});
     if(tap)out.push_back({tick,"N 6 0"});
    }else{
     for(int lane=2;lane<=4;++lane)if((mask&(2u<<lane))&&
         (fiveLane?lane!=3:!inPitch(static_cast<unsigned>(108+lane),tick)))
      out.push_back({tick,"N "+std::to_string(64+lane)+" 0"});
     // PC MIDI drum reader 0x1821383B0 enables dynamics after the text
     // event, using velocity 1 for ghost and 127 for accent. Preserve them
     // as the PC chart reader's per-lane N39/N33 marker offsets.
     const bool dynamics=std::any_of(track.texts.begin(),track.texts.end(),[&](const Text& text){return text.tick<=tick && upper(text.value)=="[ENABLE_CHART_DYNAMICS]";});
     if(dynamics)for(size_t i=begin;i<end;++i)if(gems[i].lane>=1 && (gems[i].velocity==1 || gems[i].velocity==127))
      out.push_back({tick,"N "+std::to_string(gems[i].lane+(gems[i].velocity==1?39:33))+" 0"});
    }
    previous=tick;previousMask=mask;begin=end;
   }
   for(const auto& n:track.notes){
    // Expert+ kick (pitch 95) is retained as N32, not promoted into the
    // ordinary Expert kick stream. The default PC Expert selection omits it.
    if(drums&&difficulty==3&&n.pitch==95) {
     out.push_back({n.start,"N 32 0"});globalLastTick=std::max(globalLastTick,n.start);
    }
    if(n.end<=n.start)continue;
    if(n.pitch==116||(!drums&&!six&&!hasSp116&&oldSpCount>1&&n.pitch==103))out.push_back({n.start,"S 2 "+std::to_string(n.end-n.start)});
    else if(n.pitch==103){out.push_back({n.start,"E solo"});out.push_back({n.end,"E soloend"});}
    if(drums){if(n.pitch==120)out.push_back({n.start,"S 64 "+std::to_string(n.end-n.start)});if(n.pitch==126||n.pitch==127)out.push_back({n.start,"S "+std::to_string(n.pitch==126?65:66)+" "+std::to_string(n.end-n.start)});}
   }
  }
 }
 if(!playable)return fail("MIDI contains no supported guitar, bass, rhythm, keys or drum chart.");
 if(cancelled())return fail("Install cancelled.");
 FILE* out=std::fopen(chart.c_str(),"wb");if(!out)return fail("Cannot write the imported MIDI chart.");
 // The read-only USB cache lives away from song.ini; retain the resolved
 // source threshold rather than silently falling back to a different default.
 bool ok=std::fprintf(out,"[Song]\n{\n  Resolution = %u\n  MidiImport = 1\n  MidiImportVersion = %u\n  MidiHopoThreshold = %d\n",resolution,midiImportVersion,threshold)>0;
 // PC MIDI EVENTS preserves [end]; its chart parser ignores the equivalent
 // text event. Keep that source-only property in our private cache metadata.
 if(authoredEndTick>=globalLastTick+resolution)
  if(std::fprintf(out,"  MidiEndTick = %lld\n",static_cast<long long>(authoredEndTick))<0)ok=false;
 if(std::fputs("}\n",out)<0)ok=false;
 const auto write=[&](const char* name,std::vector<ChartEvent>& events){
  std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.tick<b.tick;});
  if(std::fprintf(out,"[%s]\n{\n",name)<0)ok=false;
  for(const auto& event:events)if(std::fprintf(out,"  %lld = %s\n",static_cast<long long>(event.tick),event.value.c_str())<0)ok=false;
  if(std::fputs("}\n",out)<0)ok=false;
 };
 if(sync.empty())sync.push_back({0,"B 120000"});write("SyncTrack",sync);write("Events",global);
 for(auto& section:sections)if(!section.second.empty())write(section.first.c_str(),section.second);
 const long outputBytes=std::ftell(out);if(outputBytes<0||outputBytes>32*1024*1024)ok=false;
 if(std::fflush(out)!=0||fsync(fileno(out))!=0)ok=false;if(std::fclose(out)!=0)ok=false;
 if(!ok){unlink(chart.c_str());return fail("Cannot flush imported MIDI chart.");}
 return true;
}
} // namespace chorus
