// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include "../downloader/http_get.hpp"
#include <array>
#include <atomic>
#include <pthread.h>
#include <string>
namespace rhythm::updates {
enum class UpdateFormat {Folder,Package,Exfat};
enum class UpdateState {Idle,Checking,Current,Available,Downloading,Ready,Downloaded,Installing,RestartReady,Cancelled,Error};
struct UpdateAsset {std::string name,url,hash;uint64_t size{};};
struct ReleaseInfo {unsigned version{};std::string title,notes,tag;std::array<UpdateAsset,3> assets;};
bool parseRelease(const std::string& text,ReleaseInfo& info,std::string& error);
using UpdateFetch=bool(*)(const std::string&,chorus::HttpSink,void*,uint64_t,const std::atomic<bool>&,std::string&) noexcept;
class ApplicationUpdate {
public:
    ApplicationUpdate(unsigned current,std::string work,UpdateFetch fetch=chorus::httpGet);
    ~ApplicationUpdate();
    ApplicationUpdate(const ApplicationUpdate&)=delete;
    ApplicationUpdate& operator=(const ApplicationUpdate&)=delete;
    bool check();bool download(UpdateFormat format);bool install(std::string& error);
    bool selfTest(bool runtime=false); // Explicit QA hook; never selected by ordinary launches.
    void cancel() noexcept {cancel_=true;}
    void poll();
    UpdateState state() const noexcept {return state_.load(std::memory_order_acquire);}
    double progress() const noexcept {return progress_.load(std::memory_order_relaxed);}
    struct Snapshot {UpdateState state{};ReleaseInfo release;std::string message,path;};
    Snapshot snapshot() const;
private:
    static void* worker(void* owner);void run();bool begin(bool download);void publish(UpdateState,const std::string&);
    unsigned current_{};std::string work_;UpdateFetch fetch_;bool downloadJob_{},joinable_{},selfTestJob_{},installJob_{},runtimeTestJob_{};
    UpdateFormat format_{};pthread_t thread_{};std::atomic<bool> cancel_{false},done_{false};
    std::atomic<UpdateState> state_{UpdateState::Idle};std::atomic<double> progress_{};
    mutable pthread_mutex_t mutex_=PTHREAD_MUTEX_INITIALIZER;ReleaseInfo release_;std::string message_,path_;
};
}
// Called only before initialization and after normal title teardown on PS5.
extern "C" bool encore_recover_application_update();
extern "C" void encore_apply_application_update();

extern "C" bool encore_update_process_arguments(int argc,char** argv);
extern "C" void encore_boot_application_runtime();
extern "C" const char* encore_application_resource_root();
extern "C" void encore_application_rendered();
extern "C" void encore_restart_application_runtime();
