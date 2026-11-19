// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include "controller_profile.hpp"
#include "raw_input_queue.hpp"
#include <array>
#include <atomic>
#include <memory>
#include <pthread.h>
namespace rhythm::controls {
// PS5-only transport. Does not create virtual pads, elevate, or depend on a
// payload. An active legacy bridge retains ownership of its USB endpoint.
struct UsbSample {
    bool present{},streaming{},unknown{},bridgeOwned{};
    uint16_t vid{},pid{};int channel{-1};bool xbox{};uint64_t sequence{},receivedUs{};unsigned length{};
    std::array<uint8_t,64> report{};std::shared_ptr<const ControllerProfile> deviceProfile;
};
class Ps5UsbInput {
public:
    Ps5UsbInput();~Ps5UsbInput();
    bool start(std::string bundled,std::string local);
    void stop();
    // A batch consumer takes samples and histories atomically. Observers may
    // omit batches; observing device status must never consume gameplay input.
    std::array<UsbSample,4> snapshot(std::array<RawInputBatch,4>* batches=nullptr);
    void allow(uint16_t vid,uint16_t pid,int type,int channel=-1);
private:
    struct Device;
    static void* workerEntry(void*);void worker();
    std::string bundled_,local_;std::array<UsbSample,4> samples_{};
    std::array<RawInputQueue,4> histories_;
    std::array<std::array<int,4>,4> allowed_{};
    pthread_mutex_t mutex_{};pthread_t thread_{};
    std::atomic<bool> stopping_{false};bool running_{};
};
}
