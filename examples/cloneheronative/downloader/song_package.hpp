#pragma once
// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#include <atomic>
#include <string>
namespace chorus {
// Bump when conversion semantics change; older writable caches are refreshed.
inline constexpr unsigned midiImportVersion=5;
bool midiChartCacheCurrent(const std::string& chart) noexcept;
struct PackageMetadata {std::string title,artist,charter;};
// Extract into a private fresh staging directory and atomically publish one
// normalized song root. No rendering/network state is touched on the worker.
// Only pass a staging tree owned by the caller, never a library destination.
void discardSongPackageStage(const std::string& stage) noexcept;
bool installSongPackage(const std::string& package,const std::string& stage,
                        const std::string& destination,std::string& error,
                        const std::atomic<bool>* cancel=nullptr,
                        const PackageMetadata* metadata=nullptr) noexcept;
bool midiToChart(const std::string& midi,const std::string& chart,
                 std::string& error,const std::atomic<bool>* cancel=nullptr) noexcept;
}
