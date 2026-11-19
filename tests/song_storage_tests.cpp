#include "../examples/cloneheronative/gameplay/song_storage.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <unistd.h>
namespace fs=std::filesystem;using namespace rhythm::storage;
void write(const fs::path& p,const char* text){fs::create_directories(p.parent_path());std::ofstream(p)<<text;}
int main(){
 char temp[]="/tmp/ch-storage-XXXXXX";assert(mkdtemp(temp));fs::path root=temp,src=root/"usb/SONGS",dst=root/"internal";
 write(src/"Band/Song A/Notes.CHART","A chart");write(src/"Band/Song A/song.ogg","audio A");write(src/"Band/Song A/.hidden.ogg","hidden A");
 write(src/"Band/Song A/album.jpg","cover A");write(src/"Other/Alternate Name/notes.chart","A chart");
 write(src/"Band/Song B/notes.mid","MIDI B");write(src/"Band/Song B/drums.ogg","drum B");
 write(src/".clonehero-copy-partial/notes.chart","do not scan");fs::create_directories(dst);
 auto songs=scan({src.string()});assert(songs.size()==3);
 auto plan=planCopy(src.string(),dst.string());assert(plan.toCopy==2 && plan.duplicates==1 && plan.error.empty() && fs::is_empty(dst));
 auto future=planCopy(src.string(),(root/"not-created").string());assert(future.toCopy==2 && future.duplicates==1 && !fs::exists(root/"not-created"));
 auto r=copySongs(src.string(),dst.string());assert(r.copied==2 && r.duplicates==1 && r.failed==0);
 assert(fs::exists(dst/"Song A/album.jpg") && fs::exists(dst/"Song A/.hidden.ogg"));
 plan=planCopy(src.string(),dst.string());assert(plan.toCopy==0 && plan.duplicates==3);
 r=copySongs(src.string(),dst.string());assert(r.copied==0 && r.duplicates==3);
 write(src/"Different/Song A/notes.chart","different chart");
 r=copySongs(src.string(),dst.string());assert(r.copied==1 && r.duplicates==3 && fs::exists(dst/"Song A (2)/notes.chart"));
 assert(!equalFiles((dst/"Song A/Notes.CHART").string(),(dst/"Song A (2)/notes.chart").string()));
 std::atomic<bool> cancel{true};r=copySongs(src.string(),dst.string(),&cancel);assert(r.cancelled && r.copied==0);
 plan=planCopy(src.string(),dst.string(),&cancel);assert(plan.cancelled);
 auto absent=copySongs((root/"absent").string(),dst.string());assert(absent.failed==1);
 // Simulated unmount after the first complete song; a replacement at the
 // same pathname must not receive the remaining files.
 fs::create_directories(root/"newUSB/SONGS");bool replaced=false;
 r=copySongs(dst.string(),(root/"newUSB/SONGS").string(),nullptr,[&](unsigned done,unsigned){if(done==1&&!replaced){fs::rename(root/"newUSB",root/"removedUSB");fs::create_directories(root/"newUSB/SONGS");replaced=true;}});
 assert(replaced && r.copied==1 && r.failed==1 && fs::is_empty(root/"newUSB/SONGS"));
 auto reverse=copySongs(dst.string(),src.string());assert(reverse.copied==0 && reverse.duplicates==3);
 // Existing MIDI downloads must receive converter fixes on rescan without
 // replacing an authored chart or writing to the source/USB song directory.
 auto imported=root/"imported";
 write(imported/"notes.chart","[Song]\n{\n MidiImport = 1\n}\n[ExpertSingle]\n{\n}\n");
 write(imported/"NOTES.MID","source MIDI");
 songs=scan({imported.string()});assert(songs.size()==1 && songs[0].chart==(imported/"NOTES.MID").string());
 assert(fs::exists(imported/"notes.chart"));
 write(imported/"notes.chart","[Song]\n{\n Name = Authored\n}\n");
 songs=scan({imported.string()});assert(songs.size()==1 && songs[0].chart==(imported/"notes.chart").string());
 fs::remove(imported/"NOTES.MID");write(imported/"notes.chart","[Song]\n{\n MidiImport = 1\n}\n");
 songs=scan({imported.string()});assert(songs.size()==1 && songs[0].chart==(imported/"notes.chart").string());
 fs::remove_all(root);puts("PASS external discovery, nested MIDI/chart folders, hidden staging exclusion, complete file copies, duplicate skipping, name collisions, cancellation, bidirectional copy and removed/replaced-drive protection");
}
