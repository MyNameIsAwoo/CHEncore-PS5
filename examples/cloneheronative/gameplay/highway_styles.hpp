// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
namespace rhythm {
// Stable saved IDs follow the PC TextAsset/highways.txt catalog.
struct HighwayStyle {const char* name; const char* asset;};
inline constexpr std::array<HighwayStyle,5> highwayStyles{{
    {"Default","Texture2D/highway_board.png"},
    {"Paisley","Texture2D/Paisley.png"},
    {"Pointed Stripes","Texture2D/Pointed Stripes.png"},
    {"Chain Link","Texture2D/Chain Link.png"},
    {"Stars","Texture2D/Stars.png"}
}};
}
