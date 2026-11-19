// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <cstdint>
#include <string>
namespace rhythm::updates {
bool updatePath(const std::string& path) noexcept;
bool fileDigest(const std::string& path,std::string& digest,uint64_t* bytes=nullptr) noexcept;
bool stagePackage(const std::string& package,const std::string& work,unsigned version,std::string& error,
                  const std::atomic<bool>* cancel=nullptr,std::atomic<double>* progress=nullptr);
bool writableApplication(const std::string& root) noexcept;
bool requestInstall(const std::string& work,std::string& error);
bool applyPending(const std::string& root,const std::string& work,std::string& error);
enum class Recovery {None,Restored,Failed};
Recovery recoverPending(const std::string& root,const std::string& work,std::string& error);
// Immutable runtime generations work with writable folders and read-only titles.
// A generation is acknowledged only after the replacement draws successfully.
struct RuntimeBoot {
    std::string generation,root,executableHash;
    unsigned version{};
    bool launch{},running{},restored{};
};
bool activateRuntime(const std::string& work,std::string& error,std::atomic<double>* progress=nullptr);
bool prepareRuntime(const std::string& work,unsigned embeddedVersion,
                    const std::string& runningGeneration,RuntimeBoot& boot,std::string& error);
bool acknowledgeRuntime(const std::string& work,const std::string& generation,std::string& error);
bool rejectRuntime(const std::string& work,std::string& error);
}
