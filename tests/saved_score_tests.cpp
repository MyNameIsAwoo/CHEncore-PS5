// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/saved_score.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
    using namespace rhythm;
    FILE* file=std::tmpfile();assert(file);
    SavedScore saved;saved.score=123456789;saved.stars=7;saved.accuracyHundredths=9876;saved.controller=4;saved.player="Player One";
    assert(writeSavedScore(file,saved));std::rewind(file);SavedScore loaded;
    assert(readSavedScore(file,loaded));assert(loaded.score==saved.score && loaded.stars==7 && loaded.controller==4 && loaded.player==saved.player && loaded.details && loaded.accuracyKnown && loaded.accuracyHundredths==9876);
    std::fclose(file);
    for(const char* text:{"CHPS5SCORE 1\n900\n","CHPS5SCORE 2\n22 8 0\nName\n","CHPS5SCORE 2\n22 7 8\nName\n","CHPS5SCORE 2\n22 7 0\n","CHPS5SCORE 4\n900\n","garbage"}) {
        file=std::tmpfile();assert(file);std::fputs(text,file);std::rewind(file);
        const bool valid=readSavedScore(file,loaded);
        if(!std::strcmp(text,"CHPS5SCORE 1\n900\n"))assert(valid && loaded.score==900 && !loaded.details && loaded.player.empty());
        else assert(!valid);
        std::fclose(file);
    }
    auto lesser=saved;lesser.stars=6;assert(!betterSavedScore(lesser,saved));
    lesser=saved;lesser.accuracyHundredths=9700;assert(!betterSavedScore(lesser,saved));
    assert(!betterSavedScore(saved,saved));
    file=std::tmpfile();std::fputs("CHPS5SCORE 2\n42 5 4\nLegacy Player\n",file);std::rewind(file);
    assert(readSavedScore(file,loaded) && loaded.details && !loaded.accuracyKnown && loaded.player=="Legacy Player");std::fclose(file);
    file=std::tmpfile();saved.player="Name\nInjected";assert(!writeSavedScore(file,saved));std::fclose(file);
    std::puts("PASS saved score metadata roundtrip, legacy preservation and invalid/truncated records");
}
