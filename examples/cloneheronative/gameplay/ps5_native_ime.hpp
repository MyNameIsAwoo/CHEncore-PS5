// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace rhythm::native_ime {
// Native IME panel ABI. The public PS5 reference is AnyPS5 core/libs/SceTypes.hpp
// (Param/ImeEvent). This opens the system's panel without a CommonDialog window.
struct EditText {
    const uint16_t* text;
    uint32_t caret, areas;
    struct Area { uint32_t mode,index; int32_t length; } area[4];
};
struct Event {
    uint32_t id;
    union Data {
        struct Rect { float x,y; uint32_t width,height; } rect;
        EditText edit;
        uint64_t reserved[8];
    } data;
};
using Handler=void (*)(void*,const Event*);
struct Param {
    int32_t userId; uint32_t type; uint64_t languages;
    uint32_t enterLabel,inputMethod; void* filter;
    uint32_t option,maxLength; uint16_t* text;
    float x,y; uint32_t horizontalAlignment,verticalAlignment;
    void* work; void* arg; Handler handler; int8_t reserved[8];
};
static_assert(sizeof(Param)==96 && offsetof(Param,text)==40 && offsetof(Param,handler)==80);
static_assert(sizeof(EditText)==64 && sizeof(Event)==72 && offsetof(Event,data)==8);
struct Api {
    void (*init)(Param*);
    int (*open)(const Param*,const void*);
    int (*update)(Handler);
    int (*close)();
};
class Panel {
public:
    Api api{};
    // Waiting is separate from the system's signed error codes.
    enum Result { Waiting=-1,Accepted=0,Cancelled=1,Failed=2 };
    int open(int32_t user,uint16_t* text,uint32_t limit) noexcept {
        if(active_)return static_cast<int>(0x80bc0001u);
        if(!api.init || !api.open || !api.update || !api.close || !text || !limit)
            return static_cast<int>(0x80bc0030u);
        param_={};work_.fill(0);api.init(&param_);
        param_.userId=user;param_.maxLength=limit;param_.text=text;
        param_.x=960;param_.y=1008;
        param_.horizontalAlignment=1;param_.verticalAlignment=2;
        param_.work=work_.data();param_.arg=this;param_.handler=event;
        result_=Waiting;lastError_=0;
        const int code=api.open(&param_,nullptr);
        active_=code==0;
        return code;
    }
    Result poll() noexcept {
        if(!active_)return result_;
        if(result_==Waiting) {
            const int code=api.update(event);
            if(code<0){lastError_=code;result_=Failed;}
        }
        if(result_==Waiting)return Waiting;
        // Close after update returns, never from inside an OS callback.
        const int closed=api.close();
        if(closed<0 && static_cast<uint32_t>(closed)!=0x80bc0002u) {
            lastError_=closed;return Waiting;
        }
        active_=false;return result_;
    }
    void cancel() noexcept { if(active_)result_=Cancelled; }
    bool active() const noexcept {return active_;}
    int status() const noexcept {return active_?(result_==Waiting?1:2):0;}
    int error() const noexcept {return lastError_;}
private:
    static void event(void* arg,const Event* e) noexcept {
        if(!arg || !e)return;
        auto& self=*static_cast<Panel*>(arg);
        if(!self.active_ || self.result_!=Waiting)return;
        if(e->id==1 && e->data.edit.text) {
            uint32_t length=0;
            while(length<self.param_.maxLength && e->data.edit.text[length])++length;
            // Event text may alias the library-owned input buffer.
            std::memmove(self.param_.text,e->data.edit.text,size_t(length)*sizeof(uint16_t));
            self.param_.text[length]=0;
        } else if(e->id==5)self.result_=Accepted;
        else if(e->id==4 || e->id==6)self.result_=Cancelled;
    }
    Param param_{};
    // Stable, aligned work storage, deliberately larger than the legacy 20KiB
    // IME workspace; ownership ends only after sceImeClose succeeds.
    alignas(64) std::array<uint8_t,64*1024> work_{};
    Result result_{Waiting}; int lastError_{}; bool active_{};
};
}
