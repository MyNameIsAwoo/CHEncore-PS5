// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "../examples/cloneheronative/gameplay/pc_song_end.hpp"
#include <cassert>
#include <limits>
#include <cstdio>
int main(){
    using rhythm::pcSongFinishTime;
    assert(pcSongFinishTime(338.06360006873985,354.226)==355.226);
    assert(pcSongFinishTime(352.2455,0)==353.2455);
    assert(pcSongFinishTime(352,354,340)==341);
    assert(pcSongFinishTime(352,354,360)==355);
    assert(pcSongFinishTime(0,0)==1);
    assert(pcSongFinishTime(100,std::numeric_limits<double>::quiet_NaN())==101);
    std::puts("PASS global chart/audio endpoint, authored cutoff and one-second results delay");
}
