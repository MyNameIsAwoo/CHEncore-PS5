// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/asset_pack.hpp"
#include "gameplay/pc_asset_catalog.hpp"
#include "gameplay/update_sha256.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <unistd.h>
#include <atomic>
#define JSON_NOEXCEPTION 1
#include "json.hpp"
using Json=nlohmann::json;
namespace fs=std::filesystem;
std::string hash(const std::string& s){rhythm::updates::Sha256 h;h.add(s.data(),s.size());return h.finish();}
void pack(const fs::path& p,Json manifest,const std::string& extra="",bool corrupt=false){
 auto text=manifest.dump();std::ofstream f(p,std::ios::binary);f.write("CHASSET1",8);unsigned n=text.size();char bytes[4]={char(n),char(n>>8),char(n>>16),char(n>>24)};f.write(bytes,4);f<<text;
 for(const auto& entry:manifest["files"]){std::string content(entry["size"].get<unsigned>(),'x');if(corrupt && !content.empty())content.back()='z';f<<content;}f<<extra;
}
int main(int argc,char** argv){
 using namespace rhythm::assets;
 const auto root=fs::temp_directory_path()/("ch-asset-test-"+std::to_string(getpid()));fs::remove_all(root);fs::create_directories(root);
 const auto work=(root/"installed").string();std::string path,error;assert(!installed(work,path,error));
 if(argc>1){assert(install(argv[1],work,path,error));std::string ready;assert(installed(work,ready,error) && ready==path);fs::remove_all(root);puts("PASS actual PC importer pack: all required files installed, hashed and reloaded");return 0;}
 Json manifest={{"schema",1},{"catalog",catalog::id},{"files",Json::array()}};
 for(const auto* name:catalog::paths)manifest["files"].push_back({{"path",name},{"size",3u},{"sha256",hash("xxx")}});
 const auto file=root/"assets.pack";pack(file,manifest);std::atomic<double> progress{0};assert(install(file.string(),work,path,error,nullptr,&progress));assert(progress==1);
 const auto first=path;std::string ready;assert(installed(work,ready,error) && ready==first);
 setCurrentRoot(first);assert(currentRoot()==first+"/");
 auto rejects=[&](const Json& m,const std::string& extra="",bool corrupt=false){pack(file,m,extra,corrupt);assert(!install(file.string(),work,path,error));assert(installed(work,ready,error) && ready==first);};
 auto bad=manifest;bad["files"][0]["path"]="../escape.png";rejects(bad);assert(!fs::exists(root/"escape.png"));
 bad=manifest;bad["files"][0]["path"]="/tmp/escape.png";rejects(bad);
 bad=manifest;bad["files"][1]["path"]=bad["files"][0]["path"];rejects(bad);
 bad=manifest;bad["files"].erase(bad["files"].begin());rejects(bad);
 bad=manifest;bad["catalog"]="wrong";rejects(bad);
 bad=manifest;bad["files"][0]["sha256"]=std::string(64,'g');rejects(bad);
 rejects(manifest,"trailing bytes");rejects(manifest,"",true);
 pack(file,manifest);fs::resize_file(file,fs::file_size(file)-1);assert(!install(file.string(),work,path,error));assert(installed(work,ready,error) && ready==first);
 std::atomic<bool> cancel{true};pack(file,manifest);assert(!install(file.string(),work,path,error,&cancel));assert(installed(work,ready,error) && ready==first);
 fs::create_directories(root/"outside");fs::create_directory_symlink(root/"outside",root/"linked");assert(!install(file.string(),(root/"linked").string(),path,error));assert(fs::is_empty(root/"outside"));
 fs::create_symlink(file,root/"pack-link");assert(!install((root/"pack-link").string(),work,path,error));
 assert(install(file.string(),work,path,error) && path!=first);assert(installed(work,ready,error) && ready==path);
 auto damage=fs::path(path)/catalog::paths[0];{std::ofstream f(damage);f<<"bad";}assert(!installed(work,ready,error));
 assert(fs::exists(fs::path(first)/catalog::paths[0]));fs::remove_all(root);
 puts("PASS asset pack: exact catalog, checksums, traversal/duplicate/missing/version rejection, corruption, truncation, trailing bytes, cancellation, symlinks, atomic replacement and persistent reload");
}
