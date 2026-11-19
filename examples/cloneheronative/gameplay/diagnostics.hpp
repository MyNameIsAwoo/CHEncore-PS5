// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
namespace rhythm {
// Event-only test diagnostics. Producers never wait for file I/O or the logger.
bool startDiagnostics(const char* directory,const char* version) noexcept;
void logDiagnostic(const char* format,...) noexcept;
void stopDiagnostics() noexcept;
}
