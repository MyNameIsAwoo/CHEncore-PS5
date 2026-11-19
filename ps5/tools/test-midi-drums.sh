#!/usr/bin/env bash
# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
"${CXX:-clang++}" -std=c++20 -fno-exceptions -fno-rtti \
    -fsanitize=address,undefined -Wno-cpp -Wno-deprecated-literal-operator \
    -I examples/cloneheronative -I external/tinygltf \
    tests/midi_drums_tests.cpp examples/cloneheronative/gameplay/midi_profile.cpp \
    -o "$work/midi"
"$work/midi"
"${CXX:-clang++}" -std=c++20 -fno-exceptions -fno-rtti \
    -fsanitize=address,undefined -Wno-cpp -Wno-deprecated-literal-operator \
    -I examples/cloneheronative tests/midi_gameplay_tests.cpp \
    examples/cloneheronative/gameplay/song_pipeline.cpp -o "$work/gameplay"
"$work/gameplay"
