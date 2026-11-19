// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#define ENCORE_RUNTIME_MODULE_MAGIC UINT64_C(0x454e434f52454d31)
#define ENCORE_RUNTIME_MODULE_ABI 1u
typedef int (*EncoreRuntimeMain)(int,char**);
struct EncoreRuntimeExports {
    uint64_t magic;
    uint32_t abi,bytes;
    EncoreRuntimeMain run;
};
struct EncoreRuntimeRequest {
    uint64_t magic;
    uint32_t abi,bytes;
    struct EncoreRuntimeExports* output;
};
