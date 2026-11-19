// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <string>
namespace rhythm::assets {
// Portable, data-only asset installation. The original PC installation is read
// by the desktop importer; consoles receive only its versioned asset pack.
bool installed(const std::string& work,std::string& root,std::string& error);
bool install(const std::string& package,const std::string& work,std::string& root,std::string& error,
             const std::atomic<bool>* cancel=nullptr,std::atomic<double>* progress=nullptr);
const std::string& currentRoot();
void setCurrentRoot(std::string root);
}
