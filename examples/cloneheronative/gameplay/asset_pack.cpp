// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "asset_pack.hpp"
#include "pc_asset_catalog.hpp"
#include "file_status.hpp"
#include "update_sha256.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#define JSON_NOEXCEPTION 1
#include "json.hpp"
namespace rhythm::assets {
namespace {
using Json=nlohmann::json;
constexpr uint64_t MaxBytes=128ull*1024*1024,MaxFile=32ull*1024*1024;
constexpr size_t MaxManifest=512*1024;
struct Entry {std::string path,hash;uint64_t size{};};
std::string selectedRoot;
bool cancelled(const std::atomic<bool>* c){return c && c->load();}
bool directory(const std::string& path){struct stat s{};return fs::statusNoFollow(path.c_str(),&s)==0 && S_ISDIR(s.st_mode);}
bool parents(const std::string& path) {
 if(path.empty() || path.size()>900)return false;
 for(size_t i=1;i<path.size();++i)if(path[i]=='/'){
  const auto part=path.substr(0,i);
  if(mkdir(part.c_str(),0775) && errno!=EEXIST)return false;
  if(!directory(part))return false;
 }
 return true;
}
FILE* openRegular(const std::string& path) {
 for(size_t i=1;i<path.size();++i)if(path[i]=='/' && !directory(path.substr(0,i)))return nullptr;
 const int fd=open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return nullptr;
 struct stat s{};if(fstat(fd,&s) || !S_ISREG(s.st_mode)){close(fd);return nullptr;}
 FILE* file=fdopen(fd,"rb");if(!file)close(fd);return file;
}
bool digest(const std::string& path,std::string& hash,uint64_t& size) {
 FILE* f=openRegular(path);if(!f)return false;updates::Sha256 state;std::array<unsigned char,32768> bytes;size=0;
 for(;;){const auto n=fread(bytes.data(),1,bytes.size(),f);state.add(bytes.data(),n);size+=n;if(size>MaxFile){fclose(f);return false;}if(n<bytes.size()){const bool ok=!ferror(f);fclose(f);if(ok)hash=state.finish();return ok;}}
}
bool allowed(const std::string& path) {
 return std::any_of(std::begin(catalog::paths),std::end(catalog::paths),[&](const char* p){return path==p;});
}
bool hex(const std::string& s){return s.size()==64 && std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
bool parse(const std::string& text,std::vector<Entry>& entries) {
 const auto j=Json::parse(text,nullptr,false);
 if(j.is_discarded() || !j.is_object() || !j.count("schema") || j["schema"]!=1 ||
    !j.count("catalog") || !j["catalog"].is_string() || j["catalog"]!=catalog::id ||
    !j.count("files") || !j["files"].is_array() || j["files"].size()!=std::size(catalog::paths))return false;
 std::set<std::string> names;uint64_t total=0;
 for(const auto& value:j["files"]){
  if(!value.is_object() || !value.count("path") || !value["path"].is_string() ||
     !value.count("sha256") || !value["sha256"].is_string() ||
     !value.count("size") || !value["size"].is_number_unsigned())return false;
  Entry e{value["path"].get<std::string>(),value["sha256"].get<std::string>(),value["size"].get<uint64_t>()};
  if(!allowed(e.path) || !names.insert(e.path).second || !hex(e.hash) || !e.size || e.size>MaxFile || e.size>MaxBytes-total)return false;
  total+=e.size;entries.push_back(std::move(e));
 }
 return true;
}
bool readSmall(const std::string& path,std::string& text,size_t max) {
 FILE* f=openRegular(path);if(!f)return false;text.clear();std::array<char,4096> bytes;
 for(;;){const auto n=fread(bytes.data(),1,bytes.size(),f);text.append(bytes.data(),n);if(text.size()>max){fclose(f);return false;}if(n<bytes.size()){const bool ok=!ferror(f);fclose(f);return ok;}}
}
bool writeSmall(const std::string& path,const std::string& text) {
 const int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0664);if(fd<0)return false;
 FILE* f=fdopen(fd,"wb");if(!f){close(fd);return false;}
 bool ok=fwrite(text.data(),1,text.size(),f)==text.size();if(ok)ok=fflush(f)==0 && fsync(fd)==0;if(fclose(f))ok=false;return ok;
}
bool validGeneration(const std::string& value){return value.rfind("pack-",0)==0 && value.size()<80 && value.size()>5 && std::all_of(value.begin()+5,value.end(),[](char c){return (c>='0'&&c<='9')||c=='-';});}
void syncDirectory(const std::string& path){const int fd=open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(fd>=0){(void)fsync(fd);close(fd);}}
void cleanup(const std::string& root,const std::vector<Entry>& entries) {
 std::set<std::string> dirs;
 for(const auto& e:entries){const auto path=root+"/"+e.path;unlink(path.c_str());for(size_t i=root.size()+1;i<path.size();++i)if(path[i]=='/')dirs.insert(path.substr(0,i));}
 unlink((root+"/manifest.json").c_str());for(auto i=dirs.rbegin();i!=dirs.rend();++i)rmdir(i->c_str());rmdir(root.c_str());
}
}
const std::string& currentRoot(){return selectedRoot;}
void setCurrentRoot(std::string root){selectedRoot=std::move(root);if(!selectedRoot.empty() && selectedRoot.back()!='/')selectedRoot+='/';}
bool installed(const std::string& work,std::string& root,std::string& error) {
 root.clear();std::string active,text;
 if(!readSmall(work+"/active",active,80) || !validGeneration(active)){error="Import your PC assets to continue.";return false;}
 const auto candidate=work+"/"+active;
 std::vector<Entry> entries;
 if(!directory(candidate) || !readSmall(candidate+"/manifest.json",text,MaxManifest) || !parse(text,entries)){
  error="The installed asset pack does not match this build. Import it again.";return false;
 }
 for(const auto& e:entries){std::string hash;uint64_t bytes;
  if(!digest(candidate+"/"+e.path,hash,bytes) || hash!=e.hash || bytes!=e.size){error="An installed asset is missing or damaged. Import the pack again.";return false;}
 }
 root=candidate;error.clear();return true;
}
bool install(const std::string& package,const std::string& work,std::string& root,std::string& error,
             const std::atomic<bool>* cancel,std::atomic<double>* progress) {
 root.clear();error.clear();if(progress)*progress=0;
 FILE* input=openRegular(package);if(!input){error="The asset pack could not be opened.";return false;}
 unsigned char header[12]{};
 if(fread(header,1,sizeof(header),input)!=sizeof(header) || memcmp(header,"CHASSET1",8)){
  fclose(input);error="Choose an asset pack made by the PC importer.";return false;
 }
 const uint32_t length=uint32_t(header[8])|(uint32_t(header[9])<<8)|(uint32_t(header[10])<<16)|(uint32_t(header[11])<<24);
 if(!length || length>MaxManifest){fclose(input);error="Invalid asset manifest size.";return false;}
 std::string text(length,'\0');std::vector<Entry> entries;
 if(fread(text.data(),1,length,input)!=length || !parse(text,entries)){fclose(input);error="Asset pack version or contents do not match this build.";return false;}
 if(!parents(work+"/entry") || !directory(work)){fclose(input);error="Asset storage is unavailable.";return false;}
 static std::atomic<unsigned> serial{0};std::string generation,target;bool made=false;
 for(unsigned attempt=0;attempt<100;++attempt){generation="pack-"+std::to_string(getpid())+"-"+std::to_string(++serial);target=work+"/"+generation;if(!mkdir(target.c_str(),0775)){made=true;break;}if(errno!=EEXIST)break;}
 if(!made){fclose(input);error="Could not create an asset installation.";return false;}
 bool ok=true;uint64_t total=0,done=0;for(const auto& e:entries)total+=e.size;
 std::array<unsigned char,32768> buffer;
 for(const auto& e:entries){
  if(cancelled(cancel)){error="Asset import cancelled.";ok=false;break;}
  const auto path=target+"/"+e.path;if(!parents(path)){ok=false;break;}
  const int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0664);FILE* output=fd<0?nullptr:fdopen(fd,"wb");
  if(!output){if(fd>=0)close(fd);ok=false;break;}
  updates::Sha256 state;uint64_t left=e.size;
  while(left){if(cancelled(cancel)){ok=false;error="Asset import cancelled.";break;}
   const auto n=size_t(std::min<uint64_t>(left,buffer.size()));if(fread(buffer.data(),1,n,input)!=n || fwrite(buffer.data(),1,n,output)!=n){ok=false;break;}
   state.add(buffer.data(),n);left-=n;done+=n;if(progress)*progress=double(done)/double(total);
  }
  if(ok)ok=fflush(output)==0 && fsync(fd)==0;
  if(fclose(output))ok=false;
  if(!ok || left || state.finish()!=e.hash){ok=false;break;}
 }
 if(ok && (fgetc(input)!=EOF || ferror(input)))ok=false;
 fclose(input);
 if(ok && cancelled(cancel)){error="Asset import cancelled.";ok=false;}
 if(ok)ok=writeSmall(target+"/manifest.json",text);
 // A failed/cancelled import cannot replace an existing working pack.
 const auto temporary=work+"/active-"+generation;
 if(ok){std::set<std::string> directories;for(const auto& e:entries){const auto path=target+"/"+e.path;for(size_t i=target.size()+1;i<path.size();++i)if(path[i]=='/')directories.insert(path.substr(0,i));}
  for(const auto& path:directories)syncDirectory(path);syncDirectory(target);ok=writeSmall(temporary,generation);}
 if(ok)ok=rename(temporary.c_str(),(work+"/active").c_str())==0;
 if(!ok){unlink(temporary.c_str());cleanup(target,entries);if(error.empty())error="Asset import failed. Check the pack and available storage.";return false;}
 syncDirectory(work);root=target;if(progress)*progress=1;return true;
}
}
