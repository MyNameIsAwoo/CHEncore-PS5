// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/audio_player.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
static void checksum(std::vector<uint8_t>& bytes,size_t begin) {
    for(unsigned i=22;i<26;++i)bytes[begin+i]=0;
    uint32_t crc=0;
    for(size_t i=begin;i<bytes.size();++i){crc^=uint32_t(bytes[i])<<24;for(unsigned bit=0;bit<8;++bit)crc=(crc<<1)^((crc&0x80000000)?0x04c11db7:0);}
    for(unsigned i=0;i<4;++i)bytes[begin+22+i]=uint8_t(crc>>(i*8));
}
int main(int argc,char** argv) {
    assert(argc==3);std::ifstream input(argv[1],std::ios::binary);
    const std::vector<uint8_t> original{std::istreambuf_iterator<char>(input),{}};
    size_t last=0;for(size_t pos=0;pos<original.size();) {
        last=pos;size_t count=original[pos+26],size=27+count;
        for(size_t i=0;i<count;++i)size+=original[pos+27+i];pos+=size;assert(pos<=original.size());
    }
    assert(original[last+5]&4);
    rhythm::AudioPlayer normal;assert(normal.loadAudioFile(argv[1]));const double duration=normal.durationSeconds();
    const auto check=[&](const std::vector<uint8_t>& data,bool expected) {
        const std::string path=std::string(argv[2])+"/opus-eof-test.opus";
        {std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(data.data()),data.size());}
        rhythm::AudioPlayer player;assert(player.loadAudioFile(path.c_str())==expected);
        if(expected)assert(player.durationSeconds()==duration);else assert(!player.ready());
        rhythm::AudioPlayer excerpt;assert(excerpt.loadAudioRange(path.c_str(),0,.1)==expected);
        assert((rhythm::AudioPlayer::probeDuration(path.c_str())>0)==expected);
        assert(std::remove(path.c_str())==0);
    };
    auto bytes=original;bytes[last+5]&=~4;checksum(bytes,last);check(bytes,true);
    auto truncated=bytes;truncated.pop_back();check(truncated,false);
    auto partialHeader=bytes;partialHeader.push_back('O');check(partialHeader,false);
    auto noGranule=bytes;for(unsigned i=6;i<14;++i)noGranule[last+i]=255;checksum(noGranule,last);check(noGranule,false);
    auto impossible=bytes;for(unsigned i=6;i<14;++i)impossible[last+i]=254;checksum(impossible,last);check(impossible,false);
    puts("PASS missing-EOS Opus duration, truncated-page/header and invalid-granule rejection");
}
