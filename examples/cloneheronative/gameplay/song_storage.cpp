// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "song_storage.hpp"
#include <algorithm>
#include "file_status.hpp"
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
namespace rhythm::storage {
namespace {
bool cancelled(const std::atomic<bool>* p){return p&&p->load(std::memory_order_relaxed);}
std::string lower(std::string s){for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
bool regular(const std::string& path){struct stat st{};if(rhythm::fs::statusNoFollow(path.c_str(),&st)==0)return S_ISREG(st.st_mode);FILE* f=std::fopen(path.c_str(),"rb");if(!f)return false;std::fclose(f);return true;}
bool symlink(const std::string& path){struct stat st{};return rhythm::fs::statusNoFollow(path.c_str(),&st)==0 && S_ISLNK(st.st_mode);}
std::vector<std::string> entries(const std::string& path,bool hidden=false){
    std::vector<std::string> list;DIR* d=opendir(path.c_str());if(!d)return list;
    while(auto* e=readdir(d))if(e->d_name[0] && std::strcmp(e->d_name,".") && std::strcmp(e->d_name,"..") && (hidden || e->d_name[0]!='.'))list.emplace_back(e->d_name);
    closedir(d);std::sort(list.begin(),list.end());return list;
}
bool fingerprint(Song& song,const std::atomic<bool>* cancel){
    FILE* f=std::fopen(song.chart.c_str(),"rb");if(!f)return false;
    std::array<uint8_t,65536> b;uint64_t hash=14695981039346656037ull,total=0;bool ok=true;
    for(;;){if(cancelled(cancel)){ok=false;break;}const auto n=std::fread(b.data(),1,b.size(),f);total+=n;
        if(total>32u*1024u*1024u){ok=false;break;}for(size_t i=0;i<n;++i){hash^=b[i];hash*=1099511628211ull;}
        if(n<b.size()){ok=!std::ferror(f);break;}
    }std::fclose(f);song.hash=hash;song.bytes=total;return ok&&total>0;
}
bool importedMidiCache(const std::string& path){
    FILE* file=std::fopen(path.c_str(),"rb");if(!file)return false;
    char line[512];bool song=false,imported=false;
    for(unsigned i=0;i<64 && std::fgets(line,sizeof(line),file);++i){
        if(std::strstr(line,"[Song]"))song=true;
        else if(song && std::strchr(line,'['))break;
        if(song){char key[64]{},value[64]{};
            if(std::sscanf(line," %63[^= ] = %63s",key,value)==2 && lower(key)=="midiimport" && std::strcmp(value,"1")==0){imported=true;break;}}
    }
    std::fclose(file);return imported;
}
void walk(const std::string& folder,const std::string& root,unsigned depth,std::vector<Song>& out,const std::atomic<bool>* cancel){
    if(depth>16 || out.size()>=50000 || cancelled(cancel) || symlink(folder))return;
    const auto names=entries(folder);std::string chart,midi;int priority=99,midiPriority=99;
    for(const auto& name:names){const auto normalized=lower(name);int rank=99;
        if(normalized=="notes.chart")rank=0;else if(normalized.size()>6 && normalized.ends_with(".chart"))rank=1;
        else if(normalized=="notes.mid")rank=2;else if(normalized.ends_with(".mid")||normalized.ends_with(".midi"))rank=3;
        if(rank<priority && regular(folder+"/"+name)){priority=rank;chart=folder+"/"+name;}
        if(rank>=2 && rank<4 && rank<midiPriority && regular(folder+"/"+name)){midiPriority=rank;midi=folder+"/"+name;}
    }
    // A downloaded MIDI package retains its source next to our generated chart.
    // Re-import that source through the current converter on scan so converter
    // fixes reach existing installs. Authored charts keep their usual priority.
    if(priority<2 && !midi.empty() && importedMidiCache(chart))chart=midi;
    if(!chart.empty()){Song song{root,folder,chart};if(fingerprint(song,cancel))out.push_back(std::move(song));return;}
    for(const auto& name:names)if(directory(folder+"/"+name))walk(folder+"/"+name,root,depth+1,out,cancel);
}
// Pin the opened volume identity. A removed/replaced USB mount must not turn a
// copy into writes to a placeholder directory or to another drive at that path.
struct Lease {
    std::string root;DIR* handle{};struct stat identity{};bool valid{};
    explicit Lease(std::string path):root(std::move(path)){handle=opendir(root.c_str());valid=handle && fstat(dirfd(handle),&identity)==0;}
    ~Lease(){if(handle)closedir(handle);}
    bool current() const {
        if(!valid)return false;DIR* d=opendir(root.c_str());if(!d)return false;struct stat now{};
        const bool ok=fstat(dirfd(d),&now)==0 && now.st_dev==identity.st_dev && now.st_ino==identity.st_ino;closedir(d);return ok;
    }
};
void removeOwned(const std::string& path,unsigned depth=0){
    if(depth>20)return;DIR* d=opendir(path.c_str());if(!d){std::remove(path.c_str());return;}
    while(auto* e=readdir(d)){std::string name=e->d_name;if(name=="."||name=="..")continue;const auto child=path+"/"+name;
        if(!symlink(child)&&directory(child))removeOwned(child,depth+1);else std::remove(child.c_str());}
    closedir(d);rmdir(path.c_str());
}
bool copyTree(const std::string& from,const std::string& to,const Lease& source,const Lease& dest,const std::atomic<bool>* cancel,std::string& error,unsigned depth=0){
    if(depth>16 || cancelled(cancel)){error="Copy cancelled";return false;}
    if(!source.current() || !dest.current()){error="A song drive was removed or replaced";return false;}
    for(const auto& name:entries(from,true)){
        if(cancelled(cancel)){error="Copy cancelled";return false;}
        const auto input=from+"/"+name,output=to+"/"+name;
        if(symlink(input)){error="Song contains a symbolic link";return false;}
        if(directory(input)){
            if(mkdir(output.c_str(),0775) || !copyTree(input,output,source,dest,cancel,error,depth+1))return false;
        } else {
            FILE* in=std::fopen(input.c_str(),"rb");if(!in){error="Could not read song file";return false;}
            FILE* out=std::fopen(output.c_str(),"wb");if(!out){std::fclose(in);error="Could not create song file";return false;}
            std::array<uint8_t,256*1024> b;bool ok=true;unsigned chunks=0;
            for(;;){if(cancelled(cancel) || ((chunks++%16)==0 && (!source.current() || !dest.current()))){ok=false;error="Copy cancelled or a drive was removed";break;}
                const auto n=std::fread(b.data(),1,b.size(),in);if(n && std::fwrite(b.data(),1,n,out)!=n){ok=false;error="Could not write song file (check available space)";break;}
                if(n<b.size()){ok=!std::ferror(in);if(!ok)error="Could not finish reading song file";break;}
            }
            if(ok)ok=std::fflush(out)==0 && fsync(fileno(out))==0;
            if(std::fclose(out))ok=false;std::fclose(in);if(!ok)return false;
        }
    }return source.current() && dest.current();
}
}
bool directory(const std::string& path) noexcept {DIR* d=opendir(path.c_str());if(!d)return false;closedir(d);return true;}
bool equalFiles(const std::string& a,const std::string& b,const std::atomic<bool>* cancel){
    FILE* fa=std::fopen(a.c_str(),"rb");if(!fa)return false;FILE* fb=std::fopen(b.c_str(),"rb");if(!fb){std::fclose(fa);return false;}
    std::array<uint8_t,65536> x,y;bool same=true;
    for(;;){if(cancelled(cancel)){same=false;break;}auto nx=std::fread(x.data(),1,x.size(),fa),ny=std::fread(y.data(),1,y.size(),fb);
        if(nx!=ny || std::memcmp(x.data(),y.data(),nx)){same=false;break;}
        if(nx<x.size()){same=!std::ferror(fa)&&!std::ferror(fb);break;}
    }std::fclose(fa);std::fclose(fb);return same;
}
std::vector<Song> scan(const std::vector<std::string>& roots,const std::atomic<bool>* cancel){
    std::vector<Song> songs;for(const auto& root:roots)if(directory(root))walk(root,root,0,songs,cancel);return songs;
}
static bool knownChart(const Song& song,const std::vector<Song>& installed,
    const std::unordered_multimap<uint64_t,size_t>& known,const std::atomic<bool>* cancel) {
    const auto range=known.equal_range(song.hash);
    for(auto i=range.first;i!=range.second;++i){const auto& other=installed[i->second];if(song.bytes==other.bytes && equalFiles(song.chart,other.chart,cancel))return true;}
    return false;
}
CopyPlan planCopy(const std::string& from,const std::string& to,const std::atomic<bool>* cancel){
    CopyPlan result;
    if(from==to){result.error="Source and destination are the same library";return result;}
    const bool targetExists=directory(to);Lease source(from),dest(to);
    if(!source.valid || (targetExists && !dest.valid)){result.error="Song storage is unavailable";return result;}
    const auto songs=scan({from},cancel);auto installed=scan({to},cancel);
    std::unordered_multimap<uint64_t,size_t> known;for(size_t i=0;i<installed.size();++i)known.emplace(installed[i].hash,i);
    for(const auto& song:songs){
        if(cancelled(cancel)){result.cancelled=true;return result;}
        if(!source.current() || (targetExists && !dest.current())){result.error="A song drive was removed or replaced";return result;}
        if(knownChart(song,installed,known,cancel))++result.duplicates;
        else{++result.toCopy;known.emplace(song.hash,installed.size());installed.push_back(song);}
    }
    result.cancelled=cancelled(cancel);
    if(!source.current() || (targetExists && !dest.current()))result.error="A song drive was removed or replaced";
    return result;
}
CopyResult copySongs(const std::string& from,const std::string& to,const std::atomic<bool>* cancel,const std::function<void(unsigned,unsigned)>& progress){
    CopyResult result;if(from==to){result.error="Source and destination are the same library";result.failed=1;return result;}
    Lease source(from),dest(to);if(!source.valid || !dest.valid){result.error="Song storage is unavailable";result.failed=1;return result;}
    const auto songs=scan({from},cancel);auto installed=scan({to},cancel);
    if(cancelled(cancel)){result.cancelled=true;return result;}
    std::unordered_multimap<uint64_t,size_t> known;for(size_t i=0;i<installed.size();++i)known.emplace(installed[i].hash,i);
    unsigned completed=0;
    for(const auto& song:songs){
        if(cancelled(cancel)){result.cancelled=true;break;}
        if(!source.current() || !dest.current()){result.error="A song drive was removed or replaced";++result.failed;break;}
        if(knownChart(song,installed,known,cancel)){++result.duplicates;if(progress)progress(++completed,unsigned(songs.size()));continue;}
        const auto slash=song.folder.find_last_of('/');const auto name=song.folder.substr(slash==std::string::npos?0:slash+1);
        std::string destination=to+"/"+name;unsigned suffix=2;
        while(directory(destination) || regular(destination))destination=to+"/"+name+" ("+std::to_string(suffix++)+")";
        std::string stage;bool owned=false;
        for(unsigned attempt=0;attempt<100 && !owned;++attempt){char temp[100];std::snprintf(temp,sizeof(temp),"/.clonehero-copy-%016llx-%u",static_cast<unsigned long long>(song.hash),attempt);stage=to+temp;owned=mkdir(stage.c_str(),0775)==0;}
        if(!owned){result.error="Could not create a song staging folder";++result.failed;break;}
        std::string error;bool ok=copyTree(song.folder,stage,source,dest,cancel,error);
        // Content changed/drive removed during scanning must never publish a
        // different chart under the identity used for duplicate detection.
        const auto relative=song.chart.substr(song.folder.size());Song copied{to,stage,stage+relative};
        if(ok)ok=fingerprint(copied,cancel) && copied.bytes==song.bytes && copied.hash==song.hash && equalFiles(song.chart,copied.chart,cancel);
        bool reserved=false;
        if(ok && source.current() && dest.current() && !cancelled(cancel)) {
            for(unsigned attempt=0;attempt<100;++attempt) {
                if(mkdir(destination.c_str(),0775)==0){reserved=true;break;}
                if(errno!=EEXIST)break;
                destination=to+"/"+name+" ("+std::to_string(suffix++)+")";
            }
        }
        if(reserved && rename(stage.c_str(),destination.c_str())==0){
            Song committed{to,destination,destination+relative,song.bytes,song.hash};known.emplace(committed.hash,installed.size());installed.push_back(std::move(committed));++result.copied;
        } else {
            if(dest.current()){removeOwned(stage);if(reserved)rmdir(destination.c_str());}
            result.cancelled=cancelled(cancel);result.error=error.empty()?"Could not finish or commit song copy":error;
            if(!result.cancelled)++result.failed;break;
        }
        if(progress)progress(++completed,unsigned(songs.size()));
    }
    return result;
}
namespace {
bool relativeSongPath(const std::string& path) {
    if(path.empty() || path.front()=='/' || path.size()>400 || path.find_first_of("\\\r\n:")!=std::string::npos)return false;
    size_t start=0;while(start<path.size()){const auto end=path.find('/',start);const auto part=path.substr(start,end==std::string::npos?end:end-start);if(part.empty() || part=="." || part=="..")return false;if(end==std::string::npos)return true;start=end+1;}return false;
}
bool within(const std::string& path,const std::string& root) {return path.size()>root.size() && path.compare(0,root.size(),root)==0 && path[root.size()]=='/';}
bool songDirectoryWithoutLinks(const std::string& folder) {
    if(folder.empty() || folder.front()!='/')return false;
    std::string current;size_t start=1;
    while(start<folder.size()) {
        const auto end=folder.find('/',start);const auto part=folder.substr(start,end==std::string::npos?end:end-start);
        if(part.empty() || part=="." || part=="..")return false;
        current+="/"+part;struct stat st{};
        if(rhythm::fs::statusNoFollow(current.c_str(),&st) || !S_ISDIR(st.st_mode))return false;
        if(end==std::string::npos)return true;start=end+1;
    }return false;
}
bool writeRemoved(const std::string& registry,const std::vector<std::string>& names) {
    if(names.size()>50000)return false;
    const auto tmp=registry+".tmp";if(symlink(tmp))return false;
    FILE* out=std::fopen(tmp.c_str(),"wb");if(!out)return false;
    bool ok=std::fprintf(out,"ENCORE_REMOVED_BUNDLES 1\n")>0;
    for(const auto& name:names)ok=relativeSongPath(name) && std::fprintf(out,"%s\n",name.c_str())>0 && ok;
    ok=ok && std::fflush(out)==0 && fsync(fileno(out))==0;if(std::fclose(out))ok=false;
    if(ok)ok=std::rename(tmp.c_str(),registry.c_str())==0;
    if(!ok)(void)std::remove(tmp.c_str());return ok;
}
bool nestedChart(const std::string& folder,unsigned depth=0) {
    if(depth>32)return true;
    for(const auto& name:entries(folder,true)) {
        const auto path=folder+"/"+name;struct stat st{};if(rhythm::fs::statusNoFollow(path.c_str(),&st))return true;
        if(S_ISDIR(st.st_mode)){if(nestedChart(path,depth+1))return true;}
        else if(depth && S_ISREG(st.st_mode)){const auto n=lower(name);if(n.ends_with(".chart") || n.ends_with(".mid") || n.ends_with(".midi"))return true;}
    }return false;
}
bool eraseSongTree(const std::string& folder,unsigned depth=0) {
    if(depth>64)return false;
    bool ok=true;for(const auto& name:entries(folder,true)) {
        const auto path=folder+"/"+name;struct stat st{};if(rhythm::fs::statusNoFollow(path.c_str(),&st)){ok=false;continue;}
        if(S_ISDIR(st.st_mode))ok=eraseSongTree(path,depth+1) && ok;
        else ok=unlink(path.c_str())==0 && ok; // Unlink symlinks; never traverse them.
    }return rmdir(folder.c_str())==0 && ok;
}
}
std::vector<std::string> removedBundles(const std::string& registry) {
    std::vector<std::string> result;FILE* in=std::fopen(registry.c_str(),"rb");if(!in)return result;
    char line[512];if(!std::fgets(line,sizeof(line),in) || std::strcmp(line,"ENCORE_REMOVED_BUNDLES 1\n")){std::fclose(in);return result;}
    while(result.size()<50000 && std::fgets(line,sizeof(line),in)){std::string name=line;while(!name.empty() && (name.back()=='\n'||name.back()=='\r'))name.pop_back();if(relativeSongPath(name))result.push_back(std::move(name));}
    std::fclose(in);return result;
}
bool bundleRemoved(const std::string& folder,const std::string& bundledRoot,const std::vector<std::string>& removed) {
    if(!within(folder,bundledRoot))return false;
    const auto relative=folder.substr(bundledRoot.size()+1);
    for(const auto& old:removed)if(relative==old || within(relative,old))return true;
    return false;
}
RemoveResult removeSongs(const std::vector<std::string>& folders,const std::string& internalRoot,
    const std::string& bundledRoot,const std::string& registry,const std::atomic<bool>* cancel,
    const std::function<void(unsigned,unsigned)>& progress) {
    RemoveResult result;auto removed=removedBundles(registry);std::vector<std::string> processed;
    unsigned done=0;
    for(const auto& folder:folders) {
        if(cancelled(cancel)){result.cancelled=true;break;}
        if(std::find(processed.begin(),processed.end(),folder)!=processed.end())continue;processed.push_back(folder);
        const bool bundled=within(folder,bundledRoot),internal=within(folder,internalRoot);
        bool valid=bundled || internal;
        if(valid)valid=relativeSongPath(folder.substr((bundled?bundledRoot:internalRoot).size()+1));
        struct stat st{};
        // Walk every component without following links. Native realpath relies
        // on the same lstat operation denied to a PS5 title.
        valid=valid && songDirectoryWithoutLinks(folder) && !nestedChart(folder);
        if(!valid){++result.failed;result.error="Some selections were outside song storage or contained other songs.";continue;}
        if(bundled) {
            const auto relative=folder.substr(bundledRoot.size()+1);
            if(std::find(removed.begin(),removed.end(),relative)==removed.end())removed.push_back(relative);
            if(!writeRemoved(registry,removed)){++result.failed;result.error="Could not save bundled-song removal.";continue;}
            ++result.hidden;
            // Read-only packages retain embedded files. The saved removal also
            // prevents an update from making this song visible again.
            if(access(folder.c_str(),W_OK)==0)(void)eraseSongTree(folder);
        } else {
            // Atomically take a complete song out of the scanner before erasing.
            const auto slash=folder.find_last_of('/');const auto parent=folder.substr(0,slash);
            std::string trash;unsigned suffix=0;
            do{trash=parent+"/.encore-remove-"+std::to_string(getpid())+"-"+std::to_string(++suffix);}while(rhythm::fs::statusNoFollow(trash.c_str(),&st)==0);
            if(errno!=ENOENT || std::rename(folder.c_str(),trash.c_str())){++result.failed;result.error="Could not remove a selected song folder.";continue;}
            ++result.removed;if(!eraseSongTree(trash))++result.cleanupFailed;
        }
        if(progress)progress(++done,unsigned(folders.size()));
    }return result;
}

}
