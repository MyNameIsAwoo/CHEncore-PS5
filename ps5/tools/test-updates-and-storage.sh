#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
flags=(-std=c++20 -fno-exceptions -fno-rtti -fsanitize=address,undefined -Wno-cpp -Wno-deprecated-literal-operator -I examples/cloneheronative -I external/tinygltf -pthread)
"${CXX:-clang++}" "${flags[@]}" tests/release_version_tests.cpp -o "$work/version"
"$work/version"
for mode in normal restricted; do
extra=()
if [[ $mode == restricted ]]; then extra=(-Wl,--wrap=lstat tests/ps5_lstat_denied.cpp); fi
"${CXX:-clang++}" "${flags[@]}" "${extra[@]}" tests/song_management_tests.cpp examples/cloneheronative/gameplay/song_storage.cpp -o "$work/songs"
"$work/songs"
"${CXX:-clang++}" "${flags[@]}" "${extra[@]}" tests/update_package_tests.cpp examples/cloneheronative/gameplay/update_package.cpp -o "$work/package"
"$work/package"
"${CXX:-clang++}" "${flags[@]}" "${extra[@]}" tests/application_update_tests.cpp examples/cloneheronative/gameplay/application_update.cpp examples/cloneheronative/gameplay/update_package.cpp examples/cloneheronative/gameplay/song_storage.cpp -o "$work/updater"
"$work/updater"
done
