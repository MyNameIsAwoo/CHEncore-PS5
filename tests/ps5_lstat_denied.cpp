// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include <cerrno>
#include <sys/stat.h>
extern "C" int __wrap_lstat(const char*,struct stat*) {errno=EPERM;return -1;}
