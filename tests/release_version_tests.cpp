// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/release_version.hpp"
#include "gameplay/application_version.hpp"
#include <cassert>
#include <cstdio>
int main(){using namespace rhythm::updates;ReleaseVersion a,b,c;
 assert(releaseVersion("alpha-v39",a) && a.code==39 && !a.beta);
 assert(releaseVersion("beta-v1.0",b) && b.code==10000 && b.beta && b.label=="Beta 1.0" && b.prefix=="CH_Encore_Beta_1.0");
 assert(releaseVersion("v1.0",c) && c.code==10001 && !c.beta);
 assert(a.code<b.code && b.code<c.code);
 assert(releaseVersion("beta-v1.0.1",a) && a.code>c.code && a.code==10002);
 assert(releaseVersion("beta-v1.0.2",a) && a.code==10004);
 assert(releaseVersion("beta-v1.1",b) && b.code>a.code);
 assert(releaseVersion("beta-v2.0",c) && c.code>b.code);
 ReleaseVersion previous;assert(releaseVersion("v1.5.0",previous) && previous.code==10501);
 ReleaseVersion current;assert(releaseVersion("v1.5.1",current) && current.code==10503 && current.code==applicationVersion && current.label==applicationVersionLabel && current.prefix=="CH_Encore_1.5.1" && current.code>previous.code);
 for(const char* invalid:{"","alpha-v0","alpha-v1000","beta-v1","beta-v0.1","beta-v01.0","beta-v1.00","beta-v1.0.50","beta-v1.0.","beta-v1.0.1.2","v999.0","v1.0-rc1","../beta-v1.0"})assert(!releaseVersion(invalid,a));
 puts("PASS alpha/beta/stable ordering, patch versions, release labels/asset names and malformed version rejection");
}
