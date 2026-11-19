#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
for mode in normal restricted; do
extra=()
if [[ $mode == restricted ]]; then extra=(-Wl,--wrap=lstat tests/ps5_lstat_denied.cpp); fi
"${CXX:-clang++}" -std=c++20 -fno-exceptions -fno-rtti -fsanitize=address,undefined -I examples/cloneheronative -I external/tinygltf \
 tests/asset_pack_tests.cpp examples/cloneheronative/gameplay/asset_pack.cpp "${extra[@]}" -o "$work/pack"
"$work/pack"
if [[ $# -gt 0 ]]; then "$work/pack" "$1"; fi
done
