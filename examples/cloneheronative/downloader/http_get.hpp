// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
namespace chorus {
using HttpSink=bool(*)(const void*,size_t,void*);
// Synchronous worker-only GET. Shares network initialization with song downloads.
bool httpGet(const std::string& url,HttpSink sink,void* user,uint64_t limit,
             const std::atomic<bool>& cancel,std::string& error) noexcept;
}
