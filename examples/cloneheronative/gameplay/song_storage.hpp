// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace rhythm::storage {
struct Song {std::string root,folder,chart;uint64_t bytes{},hash{};};
struct CopyPlan {unsigned toCopy{},duplicates{};bool cancelled{};std::string error;};
struct CopyResult {unsigned copied{},duplicates{},failed{};bool cancelled{};std::string error;};
struct RemoveResult {unsigned removed{},hidden{},failed{},cleanupFailed{};bool cancelled{};std::string error;};
std::vector<std::string> removedBundles(const std::string& registry);
bool bundleRemoved(const std::string& folder,const std::string& bundledRoot,const std::vector<std::string>& removed);
RemoveResult removeSongs(const std::vector<std::string>& folders,const std::string& internalRoot,
    const std::string& bundledRoot,const std::string& registry,const std::atomic<bool>* cancel=nullptr,
    const std::function<void(unsigned,unsigned)>& progress={});
bool directory(const std::string& path) noexcept;
std::vector<Song> scan(const std::vector<std::string>& roots,const std::atomic<bool>* cancel=nullptr);
bool equalFiles(const std::string&,const std::string&,const std::atomic<bool>* cancel=nullptr);
CopyPlan planCopy(const std::string& from,const std::string& to,const std::atomic<bool>* cancel=nullptr);
CopyResult copySongs(const std::string& from,const std::string& to,const std::atomic<bool>* cancel=nullptr,
    const std::function<void(unsigned,unsigned)>& progress={});
}
