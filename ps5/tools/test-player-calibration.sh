#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
"${CXX:-clang++}" -std=c++20 -fno-exceptions -fno-rtti -fsanitize=address,undefined \
 -Wno-cpp -Wno-deprecated-literal-operator -I examples/cloneheronative -I external/tinygltf \
 tests/player_calibration_tests.cpp examples/cloneheronative/gameplay/controller_profile.cpp \
 examples/cloneheronative/gameplay/midi_profile.cpp \
 examples/cloneheronative/gameplay/song_pipeline.cpp -o "$work/calibration"
"$work/calibration"
