// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
namespace rhythm {
// Fixed memory ring; gameplay performs no file writes or allocations here.
// The adapter flushes on pause/results. Raw ages measure acquisition-to-frame
// handling only, not the controller's USB latency or display/audio latency.
struct InputDiagnostics {
    struct Record {char type{};int kind{};double time{},errorMs{};int64_t head{-1};uint64_t ageUs{};unsigned mask{};bool up{},down{},ageKnown{};};
    static constexpr unsigned capacity=128;
    std::array<Record,capacity> records;
    uint64_t samples{},judgements{},knownAges{},ageSum{},maxAge{},dropped{};
    unsigned next{},count{};bool dirty{};
    void append(Record r) noexcept {records[next]=r;next=(next+1)%capacity;count=std::min(count+1,capacity);dirty=true;}
    void sample(double time,unsigned mask,bool up,bool down,uint64_t ageUs,bool known=true) noexcept {
        ++samples;if(known){++knownAges;ageSum+=ageUs;maxAge=std::max(maxAge,ageUs);}
        append({'S',-1,time,0,-1,ageUs,mask,up,down,known});
    }
    void judgement(int kind,double time,int64_t head,double errorMs) noexcept {
        ++judgements;append({'J',kind,time,errorMs,head,0,0,false,false,false});
    }
    void write(FILE* file) const {
        std::fprintf(file,"samples %llu judgements %llu queued_age_samples %llu queued_age_mean_us %.3f queued_age_max_us %llu dropped %llu\n",
            static_cast<unsigned long long>(samples),static_cast<unsigned long long>(judgements),static_cast<unsigned long long>(knownAges),
            knownAges?double(ageSum)/knownAges:0.0,static_cast<unsigned long long>(maxAge),static_cast<unsigned long long>(dropped));
        std::fputs("type kind song_time head error_ms mask up down age_known queued_age_us\n",file);
        for(unsigned i=0;i<count;++i) {
            const auto& r=records[(next+capacity-count+i)%capacity];
            std::fprintf(file,"%c %d %.9f %lld %.6f %u %d %d %d %llu\n",r.type,r.kind,r.time,static_cast<long long>(r.head),r.errorMs,r.mask,r.up,r.down,r.ageKnown,static_cast<unsigned long long>(r.ageUs));
        }
    }
};
}
