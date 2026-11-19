// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "../examples/cloneheronative/gameplay/ps5_native_ime.hpp"
#include <cassert>
#include <cstdio>
using namespace rhythm::native_ime;
namespace {
Param request{};Event next{};bool dispatch{},updating{};int closes{},updates{},openCode{},closeCode{},updateCode{};
void init(Param* p){*p={};p->userId=-1;}
int open(const Param* p,const void*){request=*p;return openCode;}
int update(Handler cb){++updates;updating=true;if(dispatch){dispatch=false;cb(request.arg,&next);}updating=false;return updateCode;}
int close(){assert(!updating);++closes;return closeCode;}
void send(uint32_t id,const uint16_t* text=nullptr){next={};next.id=id;next.data.edit.text=text;dispatch=true;}
void reset(){closes=updates=openCode=closeCode=updateCode=0;dispatch=updating=false;}
}
int main(){
 std::array<uint16_t,4096> text{};Panel p;p.api={init,open,update,close};reset();
 assert(p.open(42,text.data(),96)==0);assert(request.userId==42 && request.maxLength==96);
 assert(request.work && reinterpret_cast<uintptr_t>(request.work)%64==0);
 assert(request.handler && request.arg==&p && request.verticalAlignment==2);
 assert(request.x>=0 && request.x<1920 && request.y>=0 && request.y<1080);
 assert(p.active() && p.status()==1 && p.poll()==Panel::Waiting && closes==0);
 const uint16_t name[]={0x41,0xe9,0xd83c,0xdfb8,0};send(1,name);
 assert(p.poll()==Panel::Waiting);assert(text[1]==0xe9 && text[2]==0xd83c && text[4]==0);
 send(5);closeCode=static_cast<int>(0x80bc0001u);
 assert(p.poll()==Panel::Waiting && p.active());const int count=updates;
 closeCode=0;assert(p.poll()==Panel::Accepted && !p.active());assert(updates==count);
 assert(p.poll()==Panel::Accepted && closes==2);
 reset();assert(p.open(42,text.data(),96)==0);send(4);
 assert(p.poll()==Panel::Cancelled && closes==1 && !p.active());
 reset();assert(p.open(42,text.data(),96)==0);send(6);
 assert(p.poll()==Panel::Cancelled && closes==1);
 reset();assert(p.open(42,text.data(),96)==0);p.cancel();send(5);
 assert(p.poll()==Panel::Cancelled && updates==0 && closes==1);dispatch=false;
 reset();assert(p.open(42,text.data(),3)==0);const uint16_t longText[]={1,2,3,4,5,0};send(1,longText);
 assert(p.poll()==Panel::Waiting && text[3]==0 && text[2]==3);
 send(1,text.data()+1);assert(p.poll()==Panel::Waiting && text[0]==2 && text[2]==0);
 p.cancel();p.poll();
 reset();assert(p.open(42,text.data(),96)==0);updateCode=static_cast<int>(0x80bc0002u);closeCode=updateCode;
 assert(p.poll()==Panel::Failed && !p.active() && p.error()==updateCode);
 reset();openCode=-7;assert(p.open(42,text.data(),96)==-7 && !p.active() && closes==0);
 Panel missing;assert(missing.open(42,text.data(),96)<0);
 std::puts("PASS native IME: events, Unicode, cancellation, aliasing, limits, close retries and API failures");
}
