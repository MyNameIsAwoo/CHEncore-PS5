// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace rhythm {
// Runtime output capability is separate from the saved preference. A saved
// 120 FPS preference falls back on a 60 Hz output without overwriting it.
// The PS5 WSI advertises high refresh only after successful mode negotiation
// and a measured vblank check; do not infer support from observed game FPS.
struct DisplayTiming {
    uint32_t refreshMilliHz{59940};
    bool supports120() const noexcept {return refreshMilliHz>=110000;}
    bool choiceEnabled(int choice) const noexcept {return choice>=0 && choice<=2 && (choice!=2 || supports120());}
    int effectiveChoice(int preference) const noexcept {return preference==2 && !supports120()?1:preference;}
    int flipRate(int preference) const noexcept {return supports120() && preference==1?1:0;}
    uint32_t targetMilliHz(int preference) const noexcept {return refreshMilliHz/static_cast<uint32_t>(flipRate(preference)+1);}
};
}
