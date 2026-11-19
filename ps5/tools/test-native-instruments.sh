#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
"${CC:-clang}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -ffunction-sections -fdata-sections -I tests/platform_stubs -Wl,--gc-sections \
  tests/ps5_native_instrument_tests.c -pthread -o "$test_dir/native"
"$test_dir/native"
"${CXX:-clang++}" -std=c++20 -fno-exceptions -fno-rtti -fsanitize=address,undefined \
  -I examples/cloneheronative -I external/tinygltf -I external/glm \
  tests/known_controller_profiles_tests.cpp examples/cloneheronative/gameplay/controller_profile.cpp \
  -o "$test_dir/profiles"
"$test_dir/profiles"

"${CXX:-clang++}" -std=c++20 -fno-exceptions -fno-rtti -fsanitize=address,undefined \
  -I examples/cloneheronative -I external/tinygltf -I external/glm \
  tests/multi_usb_input_isolation_tests.cpp examples/cloneheronative/gameplay/controller_profile.cpp \
  -o "$test_dir/isolation"
"$test_dir/isolation"
