// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "ps5_usb_input.hpp"
#include "xbox360.hpp"
#include "controller_navigation.hpp"
#include "diagnostics.hpp"
#include "usb_source_slots.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#if !defined(PS5_HOST_REFERENCE)
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include <sys/ioctl.h>
#include <poll.h>
#endif
namespace rhythm::controls {
Ps5UsbInput::Ps5UsbInput(){pthread_mutex_init(&mutex_,nullptr);for(auto& a:allowed_)a={-1,-1,-1,-1};}
Ps5UsbInput::~Ps5UsbInput(){stop();pthread_mutex_destroy(&mutex_);}
bool Ps5UsbInput::start(std::string bundled,std::string local){
    if(running_)return true;bundled_=std::move(bundled);local_=std::move(local);stopping_=false;
    running_=pthread_create(&thread_,nullptr,workerEntry,this)==0;return running_;
}
void Ps5UsbInput::stop(){if(running_){stopping_=true;pthread_join(thread_,nullptr);running_=false;}}
std::array<UsbSample,4> Ps5UsbInput::snapshot(std::array<RawInputBatch,4>* batches){
    pthread_mutex_lock(&mutex_);auto out=samples_;
    if(batches)for(unsigned i=0;i<4;++i)histories_[i].drain((*batches)[i]);
    pthread_mutex_unlock(&mutex_);return out;
}
void Ps5UsbInput::allow(uint16_t vid,uint16_t pid,int type,int channel){pthread_mutex_lock(&mutex_);for(auto& a:allowed_)if(a[0]<0 || (a[0]==vid && a[1]==pid && a[3]==channel)){a={vid,pid,type,channel};break;}pthread_mutex_unlock(&mutex_);}
void* Ps5UsbInput::workerEntry(void* p){static_cast<Ps5UsbInput*>(p)->worker();return nullptr;}
#if !defined(PS5_HOST_REFERENCE)
namespace {
uint64_t milliseconds(){using namespace std::chrono;return duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count();}
bool bridgeRunning(){
    int fd=open("/data/rhythmbridge.lock",O_RDWR|O_EXLOCK|O_NONBLOCK);
    if(fd>=0){close(fd);return false;}
    return errno==EWOULDBLOCK || errno==EAGAIN;
}
// Restrict discovery to an interrupt-IN, non-boot HID interface. Keyboard,
// mouse, audio/storage, vendor-specific interfaces and alternate settings are
// never detached or opened as rhythm-controller streams.
struct Endpoint {int iface{};uint8_t input{},output{};int protocol{};};
std::vector<Endpoint> inputEndpoints(int fd,bool wired,bool wireless){
    std::vector<Endpoint> result;uint8_t bytes[4096]{};usb_gen_descriptor d{};d.ugd_data=bytes;d.ugd_maxlen=sizeof(bytes);
    if(ioctl(fd,USB_GET_FULL_DESC,&d))return result;
    Endpoint current;bool eligible=false;
    const auto finish=[&]{if(eligible && current.input && (current.protocol!=2 || current.output))result.push_back(current);};
    for(unsigned p=0;p+2<=d.ugd_actlen;){
        const unsigned length=bytes[p];if(length<2 || p+length>d.ugd_actlen)return {};
        if(bytes[p+1]==4 && length>=9){
            finish();current={};current.iface=bytes[p+2];
            const bool alternate=bytes[p+3]!=0;const unsigned cls=bytes[p+5],sub=bytes[p+6],protocol=bytes[p+7];
            eligible=!alternate && ((cls==3 && protocol==0) || (cls==255 && sub==93 && ((wired && protocol==1)||(wireless && protocol==129))));
            current.protocol=cls==3?0:wireless?2:1;
        }
        if(eligible && bytes[p+1]==5 && length>=7 && (bytes[p+3]&3)==3){if(bytes[p+2]&128)current.input=bytes[p+2];else current.output=bytes[p+2];}
        p+=length;
    }
    finish();return result;
}
}
struct Ps5UsbInput::Device {
    int fd{-1},iface{},protocol{},subtype{};uint8_t endpoint{},outEndpoint{};bool outBusy{},profilePinned{};unsigned initOutput{};xbox360::Channel wireless;std::string path;
    std::shared_ptr<const ControllerProfile> hardwareMenu;
    UsbSample sample;usb_fs_endpoint eps[2]{};uint8_t outBuffer[12]{};void* outBuffers[2]{};uint32_t outLengths[2]{};usb_device_request keepAliveSetup{};uint64_t nextKeepAlive{};uint8_t buffer[64]{};void* buffers[1]{};uint32_t lengths[1]{};unsigned packet{},failures{};bool seen{};
    void closeStream(){if(fd>=0){usb_fs_close c{};(void)ioctl(fd,USB_FS_CLOSE,&c);usb_fs_uninit u{};(void)ioctl(fd,USB_FS_UNINIT,&u);close(fd);}fd=-1;sample.streaming=false;sample.length=0;sample.report.fill(0);}
    bool needsKeepAlive() const {return (sample.vid==0x1430 && sample.pid==0x07bb) || (sample.vid==0x12ba && sample.pid==0x074b);}
    bool openStream(int opened){
        fd=opened;buffers[0]=buffer;eps[0]={};eps[1]={};usb_fs_init init{};init.pEndpoints=eps;init.ep_index_max=(protocol==2 || needsKeepAlive())?2:1;
        if(ioctl(fd,USB_FS_INIT,&init))return false;
        usb_fs_open o{};o.ep_no=endpoint;o.max_bufsize=sizeof(buffer);o.max_frames=1;
        if(ioctl(fd,USB_FS_OPEN,&o)){
            // Only a user-approved/local or bundled raw-HID profile reaches
            // here, and a running bridge was checked before this call.
            if(ioctl(fd,USB_IFACE_DRIVER_DETACH,&iface) || ioctl(fd,USB_FS_OPEN,&o))return false;
        }
        packet=o.max_packet_length;if(!packet || packet>sizeof(buffer))return false;
        eps[0].ppBuffer=buffers;eps[0].pLength=lengths;eps[0].nFrames=1;eps[0].timeout=50;eps[0].flags=USB_FS_FLAG_SINGLE_SHORT_OK|USB_FS_FLAG_MULTI_SHORT_OK;
        lengths[0]=packet;usb_fs_start start{};if(ioctl(fd,USB_FS_START,&start))return false;
        if(protocol==2) {
            usb_fs_open output{};output.ep_index=1;output.ep_no=outEndpoint;output.max_bufsize=12;output.max_frames=1;
            if(ioctl(fd,USB_FS_OPEN,&output))return false;
            outBuffers[0]=outBuffer;eps[1].ppBuffer=outBuffers;eps[1].pLength=outLengths;eps[1].nFrames=1;eps[1].timeout=250;
            outBusy=false;initOutput=0;send(xbox360::presence);
        }
        if(needsKeepAlive()) {
            // Asynchronous endpoint-zero HID output: never block other devices
            // while a dongle acknowledges its periodic fret/strum keep-alive.
            usb_fs_open control{};control.ep_index=1;control.ep_no=0;control.max_bufsize=64;control.max_frames=2;
            if(ioctl(fd,USB_FS_OPEN,&control))return false;
            outBuffers[0]=&keepAliveSetup;outBuffers[1]=outBuffer;
            eps[1].ppBuffer=outBuffers;eps[1].pLength=outLengths;eps[1].nFrames=2;eps[1].timeout=250;
            outBusy=false;nextKeepAlive=0;
        }
        failures=0;logDiagnostic("usb stream vid=%04x pid=%04x interface=%d protocol=%d",sample.vid,sample.pid,iface,protocol);return true;
    }
    void send(const std::array<uint8_t,12>& packet){
        if(outBusy || protocol!=2)return;
        std::copy(packet.begin(),packet.end(),outBuffer);outLengths[0]=12;usb_fs_start start{};start.ep_index=1;
        outBusy=ioctl(fd,USB_FS_START,&start)==0;
    }
    void publish(const uint8_t* data,unsigned length){
        if(!length || length>sample.report.size())return;
        sample.length=length;std::copy_n(data,length,sample.report.begin());sample.streaming=true;
        if(sample.deviceProfile && sample.length!=sample.deviceProfile->reportLength)sample.unknown=true;
        sample.receivedUs=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        ++sample.sequence;failures=0;
    }
    void poll(){
        if(fd<0)return;
        if(needsKeepAlive() && !outBusy && milliseconds()>=nextKeepAlive) {
            nextKeepAlive=milliseconds()+4000;
            constexpr uint8_t ps4[]{0x30,0x02,0x08,0x0a,0,0,0,0,0};
            constexpr uint8_t ps3[]{0x02,0x08,0x20,0,0,0,0,0};
            const bool isPs4=sample.vid==0x1430;const unsigned size=isPs4?sizeof(ps4):sizeof(ps3);
            std::copy_n(isPs4?ps4:ps3,size,outBuffer);keepAliveSetup={};
            keepAliveSetup.bmRequestType=0x21;keepAliveSetup.bRequest=9;
            USETW(keepAliveSetup.wValue,0x200|outBuffer[0]);USETW(keepAliveSetup.wIndex,iface);USETW(keepAliveSetup.wLength,size);
            outLengths[0]=sizeof(keepAliveSetup);outLengths[1]=size;
            usb_fs_start start{};start.ep_index=1;outBusy=ioctl(fd,USB_FS_START,&start)==0;
            if(!outBusy)logDiagnostic("six-fret keep-alive submission failed vid=%04x pid=%04x",sample.vid,sample.pid);
        }
        usb_fs_complete done{};
        if(ioctl(fd,USB_FS_COMPLETE,&done)==0){
            if(done.ep_index==1){outBusy=false;if(protocol==2 && initOutput++==0)send(xbox360::capabilities);return;}
            if(eps[0].status==0 && lengths[0]>0 && lengths[0]<=sizeof(buffer)) {
                if(protocol==2) {
                    std::array<uint8_t,64> report{};const auto event=wireless.consume(buffer,lengths[0],report);
                    if(event==xbox360::Channel::Result::Disconnect){sample.streaming=false;sample.report.fill(0);sample.deviceProfile.reset();sample.unknown=false;}
                    else if(event==xbox360::Channel::Result::Connect){if(!outBusy)send(xbox360::capabilities);}
                    else if(event==xbox360::Channel::Result::Identity){
                        subtype=int(wireless.subtype);
                        if((subtype==6 || subtype==7) && !profilePinned) {
                            sample.deviceProfile=std::make_shared<ControllerProfile>(xbox360::profile(sample.vid,sample.pid,subtype,0));sample.unknown=false;
                            logDiagnostic("xbox receiver identity channel=%d subtype=%d",sample.channel,subtype);
                        }
                    } else if(event==xbox360::Channel::Result::Input) {
                        if(!sample.deviceProfile)sample.unknown=true;
                        publish(report.data(),20);
                    }
                } else if(protocol==1) {if(xbox360::wiredReport(buffer,lengths[0]))publish(buffer,20);}
                else if(!sample.deviceProfile || sample.deviceProfile->reportId<0 || buffer[0]==sample.deviceProfile->reportId)publish(buffer,lengths[0]);
            }
            lengths[0]=packet;usb_fs_start start{};
            if(ioctl(fd,USB_FS_START,&start) && ++failures>8)closeStream();
        } else if(errno!=EBUSY && errno!=EINTR && ++failures>8)closeStream();
    }
};
void Ps5UsbInput::worker(){
    std::array<Device,16> devices;std::array<int,16> outputSlots;outputSlots.fill(-1);uint64_t nextScan=0;
    while(!stopping_.load(std::memory_order_relaxed)){
        const auto now=milliseconds();
        if(now>=nextScan){nextScan=now+1000;for(auto& d:devices)d.seen=false;
            std::array<std::array<int,4>,4> allowed;pthread_mutex_lock(&mutex_);allowed=allowed_;pthread_mutex_unlock(&mutex_);
            // A title sandbox may hide the payload lock. Keep an explicit,
            // title-owned compatibility marker for an already-running bridge.
            bool bridge=bridgeRunning();
            if(FILE* marker=std::fopen((local_+"/legacy-bridge").c_str(),"rb")){bridge=true;std::fclose(marker);}
            DIR* dev=opendir("/dev");
            if(dev){while(auto* entry=readdir(dev)){
                if(std::strncmp(entry->d_name,"ugen",4))continue;
                const std::string path=std::string("/dev/")+entry->d_name;
                const int descriptorFd=open(path.c_str(),O_RDWR|O_NONBLOCK);if(descriptorFd<0)continue;
                usb_device_descriptor descriptor{};
                if(ioctl(descriptorFd,USB_GET_DEVICE_DESC,&descriptor)){close(descriptorFd);continue;}
                const uint16_t vid=descriptor.idVendor[0]|(descriptor.idVendor[1]<<8),pid=descriptor.idProduct[0]|(descriptor.idProduct[1]<<8);
                const int wired=xbox360::wiredType(vid,pid);const bool receiver=xbox360::receiver(vid,pid);
                const auto endpoints=inputEndpoints(descriptorFd,wired>=0,receiver);close(descriptorFd);
                for(const auto& endpoint:endpoints) {
                    auto existing=std::find_if(devices.begin(),devices.end(),[&](const Device& d){return d.sample.present && d.path==path && d.iface==endpoint.iface;});
                    if(existing!=devices.end() && existing->fd>=0){
                        auto& d=*existing;d.seen=true;char savedKey[64];
                        if(d.sample.channel>=0)std::snprintf(savedKey,sizeof(savedKey),"/%04x_%04x_ch%d.json",vid,pid,d.sample.channel);
                        else std::snprintf(savedKey,sizeof(savedKey),"/%04x_%04x.json",vid,pid);
                        ControllerProfile updated;std::string error;
                        if(loadProfile((local_+savedKey).c_str(),updated,error) && updated.vid==vid && updated.pid==pid && updated.source==Source::RawHid){
                            if(d.hardwareMenu)inheritLegacyMenuControls(updated,*d.hardwareMenu);
                            d.sample.deviceProfile=std::make_shared<ControllerProfile>(std::move(updated));d.sample.unknown=false;d.profilePinned=true;
                        } else if(d.protocol==2 && d.sample.unknown)for(const auto& permission:allowed)
                            if(permission[0]==vid && permission[1]==pid && permission[3]==d.sample.channel){
                                d.sample.deviceProfile=std::make_shared<ControllerProfile>(xbox360::profile(vid,pid,d.subtype?d.subtype:7,permission[2]));d.sample.unknown=false;d.profilePinned=true;break;
                            }
                        continue;
                    }
                    if(existing==devices.end())existing=std::find_if(devices.begin(),devices.end(),[](const Device& d){return !d.sample.present;});
                    if(existing==devices.end())break;
                    const int fd=open(path.c_str(),O_RDWR|O_NONBLOCK);if(fd<0)continue;
                    auto& d=*existing;d.seen=true;d.path=path;d.iface=endpoint.iface;d.endpoint=endpoint.input;d.outEndpoint=endpoint.output;d.protocol=endpoint.protocol;
                    d.sample.present=true;d.sample.vid=vid;d.sample.pid=pid;d.sample.xbox=endpoint.protocol!=0;d.sample.channel=endpoint.protocol==2?endpoint.iface/2:-1;
                    char key[64];
                    if(d.sample.channel>=0)std::snprintf(key,sizeof(key),"/%04x_%04x_ch%d.json",vid,pid,d.sample.channel);
                    else std::snprintf(key,sizeof(key),"/%04x_%04x.json",vid,pid);std::string error;ControllerProfile profile;
                    char variant[48]{};
                    if(vid==0x1209 && pid==0x2882)std::snprintf(variant,sizeof(variant),"/%04x_%04x_%04x.json",vid,pid,descriptor.bcdDevice[0]|(descriptor.bcdDevice[1]<<8));
                    bool mapped=false;
                    for(const auto& root:{local_,std::string("/data/cloneheronative/controllers"),bundled_}) {
                        if((variant[0] && loadProfile((root+variant).c_str(),profile,error)) || loadProfile((root+key).c_str(),profile,error)){mapped=true;break;}
                    }
                    d.hardwareMenu.reset();ControllerProfile hardware;
                    if((variant[0] && loadProfile((bundled_+variant).c_str(),hardware,error)) || loadProfile((bundled_+key).c_str(),hardware,error)) {
                        d.hardwareMenu=std::make_shared<ControllerProfile>(std::move(hardware));
                        if(mapped)inheritLegacyMenuControls(profile,*d.hardwareMenu);
                    }
                    d.profilePinned=mapped;
                    if(endpoint.protocol==1 && !mapped){profile=xbox360::profile(vid,pid,wired,wired==8?2:0);mapped=true;}
                    const auto permit=std::find_if(allowed.begin(),allowed.end(),[&](const auto& a){return a[0]==vid && a[1]==pid && a[3]==d.sample.channel;});
                    if(endpoint.protocol==2 && !mapped && permit!=allowed.end()){profile=xbox360::profile(vid,pid,7,(*permit)[2]);mapped=true;}
                    d.sample.unknown=!mapped && endpoint.protocol!=2;d.sample.bridgeOwned=bridge && vid==0x12ba && pid==0x0100;
                    if(mapped && (profile.vid!=vid || profile.pid!=pid || profile.source!=Source::RawHid)){close(fd);continue;}
                    d.sample.deviceProfile.reset();if(mapped)d.sample.deviceProfile=std::make_shared<ControllerProfile>(std::move(profile));
                    if(d.sample.bridgeOwned || (!mapped && endpoint.protocol!=2 && permit==allowed.end())){close(fd);continue;}
                    if(!d.openStream(fd))d.closeStream();
                    if(endpoint.protocol==0)break; // one raw HID input per physical instrument
                }
            }closedir(dev);}
            for(auto& d:devices)if(d.sample.present && !d.seen){d.closeStream();d.sample={};d.path.clear();}
        }
        for(auto& d:devices)d.poll();
        std::array<bool,16> active{};
        for(size_t i=0;i<devices.size();++i)active[i]=devices[i].sample.present && (devices[i].sample.streaming || devices[i].sample.unknown);
        assignUsbSources(active,outputSlots);
        pthread_mutex_lock(&mutex_);
        for(unsigned i=0;i<4;++i) {
            const auto source=std::find(outputSlots.begin(),outputSlots.end(),int(i));
            UsbSample next;if(source!=outputSlots.end())next=devices[size_t(source-outputSlots.begin())].sample;
            const auto& previous=samples_[i];
            if(!next.streaming || next.vid!=previous.vid || next.pid!=previous.pid || next.channel!=previous.channel)histories_[i].reset();
            // Sequence counters/checksums do not create input transitions.
            // The latest raw packet is still published for joystick/remapping.
            const bool changed=!previous.streaming || next.length!=previous.length ||
                (next.deviceProfile?!sameControllerInput(*next.deviceProfile,next.report.data(),next.length,previous.report.data(),previous.length):next.report!=previous.report);
            if(next.streaming && next.sequence!=previous.sequence && changed)
                histories_[i].push({next.receivedUs,next.length,next.report});
            samples_[i]=std::move(next);
        }
        pthread_mutex_unlock(&mutex_);
        usleep(2000);
    }
    for(auto& d:devices)d.closeStream();
}
#else
void Ps5UsbInput::worker(){}
#endif
}
