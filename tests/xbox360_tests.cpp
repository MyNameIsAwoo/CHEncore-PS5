// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/xbox360.hpp"
#include "gameplay/controller_navigation.hpp"
#include <cassert>
#include <cstdio>
int main(){
    using namespace rhythm::controls;using namespace xbox360;
    assert(receiver(0x045e,0x0719) && receiver(0x045e,0x02a9) && !receiver(0x1234,0x0719));
    assert(wiredType(0x1430,0x4748)==7 && wiredType(0x1bad,2)==6);
    std::array<Channel,4> channels{};std::array<uint8_t,64> report{};std::array<uint8_t,32> packet{};
    packet[0]=8;packet[1]=128;assert(channels[2].consume(packet.data(),2,report)==Channel::Result::Connect);
    assert(channels[2].connected && !channels[0].connected && !channels[1].connected && !channels[3].connected);
    packet={};packet[1]=15;packet[5]=0xcc;packet[25]=6;
    assert(channels[2].consume(packet.data(),26,report)==Channel::Result::Identity && channels[2].subtype==6);
    packet={};packet[1]=1;packet[5]=19;packet[6]=1;packet[7]=64;
    assert(channels[2].consume(packet.data(),24,report)==Channel::Result::Input && report[1]==20);
    auto rb=profile(0x045e,0x0719,6),gh=profile(0x1430,0x4748,7);
    auto rbInput=applyProfile(rb,{},report.data(),20),ghInput=applyProfile(gh,{},report.data(),20);
    assert(rbInput.frets==4 && ghInput.frets==8 && rbInput.strumUp && ghInput.strumUp);
    assert(rawMenuHat(rb,report.data(),20)==MenuHatUp);
    report[2]=8;assert(rawMenuHat(rb,report.data(),20)==MenuHatRight);
    report[2]=16;assert(applyProfile(rb,{},report.data(),20).start);
    assert(wiredReport(report.data(),20) && !wiredReport(report.data(),19));
    auto drums=profile(0x1bad,3,8,2);report[2]=0;report[3]=0x31;
    assert((applyProfile(drums,{},report.data(),20).pads&19)==19);
    packet={};packet[0]=8;assert(channels[2].consume(packet.data(),2,report)==Channel::Result::Disconnect);
    assert(!channels[2].connected && channels[2].subtype==0 && report[3]==0);
    for(unsigned n=0;n<24;++n)assert(channels[0].consume(packet.data(),n<2?n:0,report)==Channel::Result::Ignore);
    assert(presence[0]==8 && presence[2]==15 && presence[3]==192 && capabilities[2]==2);
    std::puts("PASS receiver initialization bytes, four independent channels, presence/reconnect, link subtype, 19-byte wire size, wired guitar layouts, menu navigation and drum kick/pads");
}
