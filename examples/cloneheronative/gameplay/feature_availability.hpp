// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
namespace rhythm::controls {
// Six-fret availability is additionally gated per player by controller family.
inline constexpr bool sixFretAvailable=true;
inline constexpr bool controllerTypeEnabled(int type) noexcept {
    return type>=0 && type<5 && (type!=1 || sixFretAvailable);
}
inline constexpr bool browserInstrumentEnabled(int instrument) noexcept {
    return instrument>=0 && instrument<11 && (instrument<6 || instrument==10 || sixFretAvailable);
}
}
