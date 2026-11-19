// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "ps5_usb_midi.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>
#include <unistd.h>
#if !defined(PS5_HOST_REFERENCE)
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include <dev/usb/usbdi.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <fcntl.h>
#endif
namespace rhythm::midi {
Ps5UsbMidi::Ps5UsbMidi(){pthread_mutex_init(&mutex_,nullptr);}
Ps5UsbMidi::~Ps5UsbMidi(){stop();pthread_mutex_destroy(&mutex_);}
bool Ps5UsbMidi::start(){
    if(running_)return true;
    stopping_=false;running_=pthread_create(&thread_,nullptr,entry,this)==0;return running_;
}
void Ps5UsbMidi::stop(){
    if(running_){stopping_=true;pthread_join(thread_,nullptr);running_=false;}
    pthread_mutex_lock(&mutex_);statuses_={};approvals_={};for(auto& queue:queues_)queue.reset();pthread_mutex_unlock(&mutex_);
}
std::array<UsbStatus,4> Ps5UsbMidi::snapshot(std::array<StrikeBatch,4>* batches){
    pthread_mutex_lock(&mutex_);const auto out=statuses_;
    if(batches)for(unsigned i=0;i<4;++i){
        auto& batch=(*batches)[i];batch.count=0;
        batch.dropped=queues_[i].drain([&](const Strike& hit){batch.strikes[batch.count++]=hit;});
    }
    pthread_mutex_unlock(&mutex_);return out;
}
bool Ps5UsbMidi::approve(unsigned source,uint64_t generation){
    if(source>=4 || !generation)return false;
    pthread_mutex_lock(&mutex_);
    const bool valid=statuses_[source].present && statuses_[source].generation==generation;
    if(valid)approvals_[source]=generation;
    pthread_mutex_unlock(&mutex_);return valid;
}
void* Ps5UsbMidi::entry(void* self){static_cast<Ps5UsbMidi*>(self)->worker();return nullptr;}
#if !defined(PS5_HOST_REFERENCE)
namespace {
uint64_t nowUs(){using namespace std::chrono;return uint64_t(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());}
struct Device {
    int fd{-1};std::string path;UsbStatus status;
    unsigned packet{};bool seen{},initialized{};uint64_t nextOpenUs{};
    UsbDecoder decoder;
    usb_fs_endpoint transfer{};
    std::array<uint8_t,512> buffer{};
    void* pointers[1]{};uint32_t lengths[1]{};
    void closeStream(){
        if(fd>=0){
            if(initialized){usb_fs_close closeRequest{};(void)ioctl(fd,USB_FS_CLOSE,&closeRequest);usb_fs_uninit uninit{};(void)ioctl(fd,USB_FS_UNINIT,&uninit);}
            close(fd);
        }
        fd=-1;initialized=false;status.streaming=false;decoder.reset();
    }
    bool submit(){lengths[0]=packet;usb_fs_start start{};return ioctl(fd,USB_FS_START,&start)==0;}
    bool openStream(){
        closeStream();fd=open(path.c_str(),O_RDWR|O_NONBLOCK);if(fd<0)return false;
        transfer={};usb_fs_init init{};init.pEndpoints=&transfer;init.ep_index_max=1;
        if(ioctl(fd,USB_FS_INIT,&init)){closeStream();return false;}initialized=true;
        usb_fs_open request{};request.ep_no=status.endpoint;request.max_bufsize=buffer.size();request.max_frames=1;
        if(ioctl(fd,USB_FS_OPEN,&request)){
            int interface=status.interface;
            // Only an approved, class-checked MIDIStreaming interface reaches
            // this detach. A companion gamepad and other USB interfaces stay put.
            if(ioctl(fd,USB_IFACE_DRIVER_DETACH,&interface) || ioctl(fd,USB_FS_OPEN,&request)){closeStream();return false;}
        }
        packet=request.max_packet_length;if(packet<4 || packet>buffer.size()){closeStream();return false;}
        pointers[0]=buffer.data();transfer.ppBuffer=pointers;transfer.pLength=lengths;
        transfer.nFrames=1;transfer.timeout=50;transfer.flags=USB_FS_FLAG_SINGLE_SHORT_OK|USB_FS_FLAG_MULTI_SHORT_OK;
        if(!submit()){closeStream();return false;}status.streaming=true;return true;
    }
    template<class Emit> void poll(Emit&& emit){
        // Bound draining so one noisy MIDI endpoint cannot starve other kits.
        for(unsigned n=0;fd>=0 && n<8;++n){
            usb_fs_complete completion{};
            if(ioctl(fd,USB_FS_COMPLETE,&completion)){
                if(errno!=EBUSY && errno!=EINTR && errno!=EAGAIN)closeStream();
                break;
            }
            if(transfer.status==USB_ERR_NORMAL_COMPLETION){
                if(lengths[0]>buffer.size()){closeStream();break;}
                decoder.feed({buffer.data(),lengths[0]},nowUs(),emit);
            }else if(transfer.status!=USB_ERR_TIMEOUT){closeStream();break;}
            if(!submit()){closeStream();break;}
        }
    }
};
}
void Ps5UsbMidi::worker(){
    std::array<Device,4> devices;uint64_t nextScan=0,generation=0;
    while(!stopping_.load(std::memory_order_relaxed)){
        const auto now=nowUs();
        if(now>=nextScan){
            nextScan=now+1000000;for(auto& device:devices)device.seen=false;
            if(DIR* directory=opendir("/dev")){
                while(auto* entry=readdir(directory)){
                    if(std::strncmp(entry->d_name,"ugen",4))continue;
                    const auto path=std::string("/dev/")+entry->d_name;
                    const int fd=open(path.c_str(),O_RDWR|O_NONBLOCK);if(fd<0)continue;
                    usb_device_descriptor descriptor{};usb_gen_descriptor configuration{};
                    std::array<uint8_t,4096> bytes{};configuration.ugd_data=bytes.data();configuration.ugd_maxlen=bytes.size();
                    const bool read=ioctl(fd,USB_GET_DEVICE_DESC,&descriptor)==0 && ioctl(fd,USB_GET_FULL_DESC,&configuration)==0;
                    close(fd);if(!read || configuration.ugd_actlen>bytes.size())continue;
                    std::array<UsbInputEndpoint,4> endpoints{};
                    const auto count=inputEndpoints({bytes.data(),configuration.ugd_actlen},endpoints);
                    for(unsigned n=0;n<count;++n){
                        const auto& endpoint=endpoints[n];
                        const uint16_t vid=UGETW(descriptor.idVendor),pid=UGETW(descriptor.idProduct);
                        auto found=std::find_if(devices.begin(),devices.end(),[&](const auto& device){
                            return device.status.present && device.path==path && device.status.vid==vid && device.status.pid==pid &&
                                device.status.interface==endpoint.interface && device.status.endpoint==endpoint.address;
                        });
                        if(found==devices.end()){
                            found=std::find_if(devices.begin(),devices.end(),[](const auto& device){return !device.status.present;});
                            if(found==devices.end())continue;
                            found->path=path;found->status={true,false,vid,pid,endpoint.interface,endpoint.address,++generation};
                        }
                        found->seen=true;
                    }
                }
                closedir(directory);
            }
            for(auto& device:devices)if(device.status.present && !device.seen){device.closeStream();device.status={};device.path.clear();}
            pthread_mutex_lock(&mutex_);
            for(unsigned i=0;i<4;++i){
                if(statuses_[i].generation!=devices[i].status.generation){queues_[i].reset();approvals_[i]=0;}
                statuses_[i]=devices[i].status;
            }
            pthread_mutex_unlock(&mutex_);
        }
        for(unsigned i=0;i<4;++i){
            auto& device=devices[i];bool approved;
            pthread_mutex_lock(&mutex_);approved=device.status.present && approvals_[i]==device.status.generation;pthread_mutex_unlock(&mutex_);
            if(!approved)continue;
            if(device.fd<0){if(now<device.nextOpenUs)continue;device.nextOpenUs=now+1000000;(void)device.openStream();}
            StrikeBatch batch;
            device.poll([&](const Strike& hit){if(batch.count<batch.strikes.size())batch.strikes[batch.count++]=hit;else ++batch.dropped;});
            pthread_mutex_lock(&mutex_);statuses_[i]=device.status;
            for(unsigned n=0;n<batch.count;++n)queues_[i].push(batch.strikes[n]);
            queues_[i].accountDropped(batch.dropped);
            pthread_mutex_unlock(&mutex_);
        }
        const bool active=std::any_of(devices.begin(),devices.end(),[](const auto& d){return d.status.streaming;});
        usleep(active?1000:20000);
    }
    for(auto& device:devices)device.closeStream();
}
#else
void Ps5UsbMidi::worker(){}
#endif
}
