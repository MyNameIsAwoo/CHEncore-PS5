// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#include "../examples/cloneheronative/downloader/song_package.hpp"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
int main(int argc,char** argv){
 if(argc==3 && std::strcmp(argv[1],"cache")==0)return chorus::midiChartCacheCurrent(argv[2])?0:1;
 if(argc<4)return 2;
 std::string error;std::atomic<bool> cancel{argc>5};
 if(std::strcmp(argv[1],"midi")==0){if(!chorus::midiToChart(argv[2],argv[3],error,&cancel)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}return 0;}
 if(argc<5)return 2;
 if(mkdir(argv[3],0700)!=0)return 2;
 if(!chorus::installSongPackage(argv[2],argv[3],argv[4],error,&cancel)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
 return 0;
}
