// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include "midi_drums.hpp"
#include <array>
#include <atomic>
#include <pthread.h>

namespace rhythm::midi {
struct UsbStatus {
    bool present{},streaming{};
    uint16_t vid{},pid{};
    uint8_t interface{},endpoint{};
    uint64_t generation{};
};
struct StrikeBatch {
    std::array<Strike,512> strikes{};
    unsigned count{},dropped{};
};
// Auxiliary MIDI sources attach to a DualSense player; they do not consume or
// synthesize scePad slots. Discovery never claims storage/HID/audio interfaces.
class Ps5UsbMidi {
public:
    Ps5UsbMidi();~Ps5UsbMidi();
    Ps5UsbMidi(const Ps5UsbMidi&)=delete;
    Ps5UsbMidi& operator=(const Ps5UsbMidi&)=delete;
    bool start();void stop();
    std::array<UsbStatus,4> snapshot(std::array<StrikeBatch,4>* batches=nullptr);
    // Called after kit selection or a recognized saved assignment. The
    // generation prevents a queued approval from claiming a replacement device.
    bool approve(unsigned source,uint64_t generation);
private:
    static void* entry(void*);void worker();
    std::array<UsbStatus,4> statuses_{};
    std::array<StrikeQueue<>,4> queues_{};
    std::array<uint64_t,4> approvals_{};
    pthread_mutex_t mutex_{};pthread_t thread_{};
    std::atomic<bool> stopping_{false};bool running_{};
};
}
