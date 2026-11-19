// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
// Link the actual PS5 downloader against a deterministic HTTPS service fixture.
#include "../examples/cloneheronative/downloader/chorus_downloader.hpp"
#include <cassert>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>
#include <sys/stat.h>
namespace {
std::string archive,response,currentUrl;std::vector<std::string> urls;size_t offset=0;int code=200;bool zipMode=false;
std::atomic<bool> aborted{false};
void complete(chorus::Downloader& d,chorus::Downloader::State state){
 for(int i=0;i<5000;++i){d.update();if(d.state()==state)return;if(d.state()==chorus::Downloader::State::Error){std::fprintf(stderr,"%s\n",d.error());break;}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 assert(d.state()==state);
}
}
extern "C" {
int sceNetInit(){return 0;}int sceNetPoolCreate(const char*,int,int){return 1;}int sceSslInit(size_t){return 1;}int sceHttp2Init(int,int,size_t,int){return 1;}
int sceHttp2CreateTemplate(int,const char*,int,int){return 1;}
int sceHttp2CreateRequestWithURL(int,const char* method,const char* url,uint64_t){currentUrl=url;urls.emplace_back(url);offset=0;aborted=false;code=(zipMode&&currentUrl.find(".sng")!=std::string::npos)?404:200;if(std::string(method)=="POST")response=R"({"found":1,"data":[{"md5":"0123456789abcdef0123456789abcdef","encoderVersion":3,"name":"Fixture","artist":"Tests","charter":"Fixture author","hasVideoBackground":true,"driveFileName":"fixture.zip","driveFileId":"Fixture_Zip-123","driveChartIsPack":false}]})";else response=archive;return 1;}
int sceHttp2AddRequestHeader(int,const char*,const char*,unsigned){return 0;}int sceHttp2SetConnectTimeOut(int,uint64_t){return 0;}int sceHttp2SetRecvTimeOut(int,uint64_t){return 0;}int sceHttp2SetSendTimeOut(int,uint64_t){return 0;}int sceHttp2SendRequest(int,const void*,size_t){return 0;}int sceHttp2GetStatusCode(int,int* out){*out=code;return 0;}
int sceHttp2ReadData(int,void* out,size_t n){if(aborted)return -1;n=std::min(n,response.size()-offset);std::memcpy(out,response.data()+offset,n);offset+=n;return static_cast<int>(n);}
int sceHttp2AbortRequest(int){aborted=true;return 0;}int sceHttp2DeleteRequest(int){return 0;}int sceHttp2DeleteTemplate(int){return 0;}
}
int main(int argc,char** argv){
 assert(argc==4);zipMode=std::string(argv[1])=="zip";
 std::ifstream file(argv[2],std::ios::binary);archive.assign(std::istreambuf_iterator<char>(file),{});assert(!archive.empty());
 chorus::Downloader d(argv[3]);assert(d.search("fixture"));complete(d,chorus::Downloader::State::Results);assert(d.resultCount()==1);
 assert(d.download(0));complete(d,chorus::Downloader::State::Complete);
 const std::string expected=std::string(argv[3])+"/Fixture_01234567";assert(d.installedPath()==expected);
 struct stat st{};assert(stat((expected+"/notes.chart").c_str(),&st)==0);
 if(zipMode){std::ifstream ini(expected+"/song.ini");const std::string text(std::istreambuf_iterator<char>(ini),{});assert(text.find("name = Fixture")!=std::string::npos);}
 assert(urls[1]=="https://files.enchor.us/charts/v3/0123456789abcdef0123456789abcdef_novideo.sng");
 if(zipMode){assert(urls.size()==4);assert(urls[2]=="https://files.enchor.us/charts/v3/0123456789abcdef0123456789abcdef.sng");assert(urls[3].find("drive.usercontent.google.com/download?")==8);}
 assert(d.download(0));assert(d.state()==chorus::Downloader::State::Complete);assert(urls.size()==(zipMode?4:2));
 d.reset();assert(d.state()==chorus::Downloader::State::Idle&&d.resultCount()==0&&d.page()==0&&!d.hasMoreResults());assert(!*d.error()&&!*d.installedPath()&&d.progress()==0);
 assert(d.search("again"));d.reset();assert(d.state()==chorus::Downloader::State::Idle&&d.resultCount()==0);
 std::puts("PASS HTTPS fallback, worker install, repeat install and reset/cancellation");
}
