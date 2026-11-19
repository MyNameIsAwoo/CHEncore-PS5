// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "diagnostics.hpp"
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
namespace rhythm { namespace {
constexpr size_t capacity=256,lineSize=384,maxBytes=512*1024;
struct Recorder {
    pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t wake=PTHREAD_COND_INITIALIZER;
    pthread_t thread{};bool joinable{};std::atomic<bool> running{},stopping{};std::atomic<unsigned> dropped{};
    std::array<std::array<char,lineSize>,capacity> queue{};size_t head{},count{};
    char path[512]{};double started{};
};
Recorder recorder;
double seconds(){timespec ts{};clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}
void rotate(){
    char first[520],second[520];std::snprintf(first,sizeof(first),"%s.1",recorder.path);std::snprintf(second,sizeof(second),"%s.2",recorder.path);
    (void)std::remove(second);(void)std::rename(first,second);(void)std::rename(recorder.path,first);
}
void* worker(void*){
    FILE* file=std::fopen(recorder.path,"ab");if(!file){recorder.running=false;return nullptr;}
    struct stat info{};size_t bytes=fstat(fileno(file),&info)==0?size_t(info.st_size):0;double synced=seconds();bool dirty=false;
    for(;;){
        std::array<std::array<char,lineSize>,32> batch{};size_t count=0;
        pthread_mutex_lock(&recorder.lock);
        if(!recorder.count && !recorder.stopping){timespec until{};clock_gettime(CLOCK_REALTIME,&until);until.tv_nsec+=500000000;if(until.tv_nsec>=1000000000){++until.tv_sec;until.tv_nsec-=1000000000;}pthread_cond_timedwait(&recorder.wake,&recorder.lock,&until);}
        while(count<batch.size() && recorder.count){batch[count++]=recorder.queue[recorder.head];recorder.head=(recorder.head+1)%capacity;--recorder.count;}
        const bool stop=recorder.stopping && recorder.count==0;
        pthread_mutex_unlock(&recorder.lock);
        if(bytes>=maxBytes){std::fclose(file);rotate();file=std::fopen(recorder.path,"wb");bytes=0;if(!file)break;}
        const unsigned dropped=recorder.dropped.exchange(0);
        if(dropped){bytes+=std::fprintf(file,"diagnostic events dropped: %u\n",dropped);dirty=true;}
        for(size_t i=0;i<count;++i){const size_t length=std::strlen(batch[i].data());bytes+=std::fwrite(batch[i].data(),1,length,file);dirty=true;}
        if(dirty)std::fflush(file);
        if(dirty && (stop || seconds()-synced>=2)){(void)fsync(fileno(file));synced=seconds();dirty=false;}
        if(stop)break;
    }
    if(file)std::fclose(file);return nullptr;
}
}
bool startDiagnostics(const char* directory,const char* version) noexcept {
    if(recorder.joinable || !directory || !version)return false;
    (void)mkdir(directory,0777);const int length=std::snprintf(recorder.path,sizeof(recorder.path),"%s/encore-test.log",directory);
    if(length<=0 || size_t(length)>=sizeof(recorder.path))return false;
    recorder.head=recorder.count=0;recorder.dropped=0;recorder.stopping=false;recorder.started=seconds();recorder.running=true;
    if(pthread_create(&recorder.thread,nullptr,worker,nullptr)){recorder.running=false;return false;}
    recorder.joinable=true;
    const auto now=std::time(nullptr);logDiagnostic("SESSION START build=%s utc_epoch=%lld; event log; no raw controller/audio recording",version,static_cast<long long>(now));return true;
}
void logDiagnostic(const char* format,...) noexcept {
    if(!recorder.running || !format)return;
    std::array<char,lineSize> line{};const int prefix=std::snprintf(line.data(),line.size(),"+%.3f ",seconds()-recorder.started);
    va_list args;va_start(args,format);std::vsnprintf(line.data()+prefix,line.size()-size_t(prefix)-2,format,args);va_end(args);
    auto end=std::strlen(line.data());for(size_t i=0;i<end;++i)if(line[i]=='\n' || line[i]=='\r')line[i]=' ';
    line[end++]='\n';line[end]=0;
    if(pthread_mutex_trylock(&recorder.lock)){++recorder.dropped;return;}
    if(recorder.count==capacity)++recorder.dropped;
    else {recorder.queue[(recorder.head+recorder.count)%capacity]=line;++recorder.count;}
    pthread_mutex_unlock(&recorder.lock);pthread_cond_signal(&recorder.wake);
}
void stopDiagnostics() noexcept {
    if(!recorder.joinable)return;
    logDiagnostic("SESSION END clean shutdown");recorder.running=false;recorder.stopping=true;pthread_cond_signal(&recorder.wake);
    pthread_join(recorder.thread,nullptr);recorder.joinable=false;
}
}
