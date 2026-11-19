// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/chart_catalog.hpp"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
int main(){
 char path[]="/tmp/ch-catalog-XXXXXX";int fd=mkstemp(path);assert(fd>=0);FILE* f=fdopen(fd,"w");assert(f);
 fputs("[ExpertSingle]\n{\n 0 = N 6 0\n}\n[HardSingle]\n{\n 192 = N 0 0\n}\n[ExpertDoubleBass]\n{\n 192 = N 7 0\n}\n[ExpertDrums]\n{\n 192 = N 5 0\n}\n[MediumKeyboard]\n{\n 192 = N 2 0\n}\n",f);fclose(f);
 const auto catalog=rhythm::chartCatalog(path);unlink(path);
 assert(catalog.difficulties[0]==4 && catalog.difficulties[3]==8 && catalog.difficulties[4]==8 && catalog.difficulties[5]==2);
 assert(catalog.instruments==((1<<0)|(1<<3)|(1<<4)|(1<<5)));
 assert(rhythm::chartCatalog(path).instruments==0);
 puts("PASS cached chart availability: populated difficulties, open bass, kick and empty marker-only sections");
}
