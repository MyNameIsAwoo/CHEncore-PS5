#include "audio_player.hpp"
#include "menu_preview.hpp"
#include "audio_stream.hpp"
#include <pthread.h>
#include <unistd.h>
#include <new>

#include <opus/opus.h>
#define STB_VORBIS_HEADER_ONLY
#include "../../../external/song_audio/stb_vorbis.c"
#include "../../../external/song_audio/dr_mp3.h"
#include "../../../external/song_audio/dr_wav.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <dirent.h>
#include <string>
#include <vector>
#include <cctype>
#include <time.h>

namespace rhythm {
namespace {
// Decoding remains bounded by each player/mixer PCM budget. The original
// bundled set includes a 20-minute chart; a 15-minute time cap rejected it.
constexpr uint64_t MaxSongFrames = uint64_t{48000} * 60 * 60;
constexpr uint64_t AudioPhaseOne = uint64_t{1} << 32;
constexpr uint64_t MaxEncodedBytes = uint64_t{256} * 1024 * 1024;
constexpr size_t MaxPacketBytes = 1024 * 1024;
constexpr int MaxDecodedFrames = 5760; // Opus' maximum 120 ms packet at 48 kHz.

uint32_t readU32(const uint8_t* p) noexcept {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

uint64_t readU64(const uint8_t* p) noexcept {
    return uint64_t(readU32(p)) | (uint64_t(readU32(p + 4)) << 32);
}

bool readExact(FILE* file, void* output, size_t bytes) noexcept {
    return bytes == 0 || std::fread(output, 1, bytes, file) == bytes;
}

bool appendFrames(int16_t*& pcm, uint64_t& count, uint64_t& capacity,
                  const int16_t* decoded, int frames, int channels, uint64_t frameLimit) noexcept {
    if (frames <= 0 || (channels != 1 && channels != 2)) return false;
    const uint64_t next = count + static_cast<uint64_t>(frames);
    frameLimit = std::min(frameLimit, MaxSongFrames);
    if (next < count || next > frameLimit) return false;
    if (next > capacity) {
        uint64_t grown = capacity == 0 ? std::min<uint64_t>(48000u * 30u, frameLimit) : capacity;
        while (grown < next) {
            const uint64_t step = std::max<uint64_t>(grown / 2, 48000u * 10u);
            if (step > frameLimit - grown) { grown = frameLimit; break; }
            grown += step;
        }
        if (grown < next || grown > std::numeric_limits<size_t>::max() / (2 * sizeof(int16_t))) return false;
        void* allocation = std::realloc(pcm, static_cast<size_t>(grown) * 2 * sizeof(int16_t));
        if (!allocation) return false;
        pcm = static_cast<int16_t*>(allocation);
        capacity = grown;
    }
    int16_t* target = pcm + count * 2;
    if (channels == 2) {
        std::memcpy(target, decoded, static_cast<size_t>(frames) * 2 * sizeof(int16_t));
    } else {
        for (int i = 0; i < frames; ++i) target[i * 2] = target[i * 2 + 1] = decoded[i];
    }
    count = next;
    return true;
}

// Archive originals commonly use 44.1-kHz Vorbis/MP3/WAV. Keep the same
// bounded 48-kHz stereo PCM contract as Opus, without work in audioFill.
struct Resample48 {
    int16_t*& pcm;uint64_t& frames;uint64_t limit,capacity=0;
    unsigned rate;int channels;uint64_t sourceFrames=0,next=0;
    std::array<int16_t,2> previous{};
    std::array<int16_t,8192> output{};size_t count=0;uint64_t stopAfter=UINT64_MAX;
    bool done() const noexcept {return frames+count>=stopAfter;}
    bool flush(){if(!count)return true;const bool ok=appendFrames(pcm,frames,capacity,output.data(),static_cast<int>(count),2,limit);count=0;return ok;}
    bool emit(int16_t left,int16_t right){output[count*2]=left;output[count*2+1]=right;if(++count==output.size()/2)return flush();return true;}
    bool feed(const int16_t* data,size_t n){
        if(rate==48000){n=std::min<uint64_t>(n,stopAfter-frames);return !n || appendFrames(pcm,frames,capacity,data,static_cast<int>(n),channels,limit);}
        for(size_t i=0;i<n && !done();++i){
            const std::array<int16_t,2> current={data[i*static_cast<size_t>(channels)],data[i*static_cast<size_t>(channels)+(channels==2?1:0)]};
            if(!sourceFrames)previous=current;
            const uint64_t edge=sourceFrames*48000;
            while(next<=edge && !done()){
                const uint64_t fraction=sourceFrames?next-(sourceFrames-1)*48000:48000;
                const auto sample=[&](size_t ch){return static_cast<int16_t>((int64_t(previous[ch])*static_cast<int64_t>(48000-fraction)+int64_t(current[ch])*static_cast<int64_t>(fraction))/48000);};
                if(!emit(sample(0),sample(1)))return false;
                next+=rate;
            }
            previous=current;++sourceFrames;
        }
        return true;
    }
    bool finish(){
        if(rate==48000)return frames>0;
        while(next<sourceFrames*48000 && !done()){if(!emit(previous[0],previous[1]))return false;next+=rate;}
        return flush()&&frames>0;
    }
};
bool decodeArchiveAudio(const char* path,int16_t*& pcm,uint64_t& frames,uint64_t limit) noexcept {
    std::array<int16_t,8192> data{};
    const auto valid=[](unsigned rate,int channels){return rate>=8000&&rate<=192000&&(channels==1||channels==2);};
    FILE* input=std::fopen(path,"rb");if(!input)return false;
    if(std::fseek(input,0,SEEK_END)!=0){std::fclose(input);return false;}
    const long size=std::ftell(input);uint8_t magic[12]{};
    const bool readable=size>0&&static_cast<uint64_t>(size)<=MaxEncodedBytes&&std::fseek(input,0,SEEK_SET)==0&&std::fread(magic,1,sizeof(magic),input)==sizeof(magic);
    std::fclose(input);if(!readable)return false;
    if(!std::memcmp(magic,"OggS",4)){
        int error=0;stb_vorbis* decoder=stb_vorbis_open_filename(path,&error,nullptr);if(!decoder)return false;
        const auto info=stb_vorbis_get_info(decoder);bool ok=valid(info.sample_rate,info.channels);
        Resample48 resample{pcm,frames,limit,0,info.sample_rate,info.channels};
        while(ok){const int count=stb_vorbis_get_samples_short_interleaved(decoder,info.channels,data.data(),static_cast<int>(data.size()));if(count<=0)break;ok=resample.feed(data.data(),static_cast<size_t>(count));}
        ok=ok&&stb_vorbis_get_error(decoder)==0&&resample.finish();stb_vorbis_close(decoder);return ok;
    }
    if(!std::memcmp(magic,"RIFF",4)&&!std::memcmp(magic+8,"WAVE",4)){
        drwav decoder{};if(!drwav_init_file(&decoder,path,nullptr))return false;
        bool ok=valid(decoder.sampleRate,static_cast<int>(decoder.channels));
        Resample48 resample{pcm,frames,limit,0,decoder.sampleRate,static_cast<int>(decoder.channels)};
        while(ok){const auto count=drwav_read_pcm_frames_s16(&decoder,data.size()/decoder.channels,data.data());if(!count)break;ok=resample.feed(data.data(),static_cast<size_t>(count));}
        ok=ok&&resample.finish();drwav_uninit(&decoder);return ok;
    }
    drmp3 decoder{};if(!drmp3_init_file(&decoder,path,nullptr))return false;
    bool ok=valid(decoder.sampleRate,static_cast<int>(decoder.channels));
    Resample48 resample{pcm,frames,limit,0,decoder.sampleRate,static_cast<int>(decoder.channels)};
    while(ok){const auto count=drmp3_read_pcm_frames_s16(&decoder,data.size()/decoder.channels,data.data());if(!count)break;ok=resample.feed(data.data(),static_cast<size_t>(count));}
    ok=ok&&resample.finish();drmp3_uninit(&decoder);return ok;
}

struct AudioInfo {uint64_t frames48{};unsigned rate{};int channels{};bool opus{};uint64_t nativeFrames{};};
bool probeAudio(const char* path,AudioInfo& info) noexcept {
    FILE* file=std::fopen(path,"rb");if(!file)return false;
    if(std::fseek(file,0,SEEK_END)){std::fclose(file);return false;}
    const long length=std::ftell(file);uint8_t header[27]{};
    if(length<27 || uint64_t(length)>MaxEncodedBytes || std::fseek(file,0,SEEK_SET) || !readExact(file,header,sizeof(header))){std::fclose(file);return false;}
    if(!std::memcmp(header,"OggS",4) && !header[4] && (header[5]&2)) {
        uint8_t laces[255]{};uint8_t identity[19]{};
        if(readExact(file,laces,header[26]) && header[26] && laces[0]>=19 && readExact(file,identity,sizeof(identity)) && !std::memcmp(identity,"OpusHead",8)) {
            const unsigned preSkip=identity[10]|(unsigned(identity[11])<<8);
            const uint32_t serial=readU32(header+14);
            const size_t count=std::min<size_t>(uint64_t(length),128*1024);
            std::vector<uint8_t> tail(count);uint64_t granule=UINT64_MAX;
            if(identity[8]<=15 && (identity[9]==1 || identity[9]==2) && identity[18]==0 &&
               !std::fseek(file,length-static_cast<long>(count),SEEK_SET) && readExact(file,tail.data(),count)) {
                for(size_t p=0;p+27<=count;++p) {
                    const auto* page=tail.data()+p;
                    if(std::memcmp(page,"OggS",4) || page[4] || readU32(page+14)!=serial)continue;
                    const size_t segments=page[26];if(p+27+segments>count)continue;
                    size_t bytes=27+segments;for(size_t j=0;j<segments;++j)bytes+=page[27+j];
                    // A missing-EOS source must end on a complete page;
                    // do not mistake a truncated final page for a shorter song.
                    if(bytes>count-p || (p+bytes!=count && !(page[5]&4)))continue;
                    const auto at=readU64(page+6);if(at!=UINT64_MAX)granule=at;
                }
            }
            std::fclose(file);
            if(granule==UINT64_MAX || granule<=preSkip || granule-preSkip>MaxSongFrames)return false;
            info={granule-preSkip,48000,identity[9],true};return true;
        }
    }
    std::fclose(file);
    uint64_t frames=0;unsigned rate=0;int channels=0;
    if(!std::memcmp(header,"OggS",4)) {
        int error=0;auto* decoder=stb_vorbis_open_filename(path,&error,nullptr);if(!decoder)return false;
        const auto data=stb_vorbis_get_info(decoder);rate=data.sample_rate;channels=data.channels;
        frames=stb_vorbis_stream_length_in_samples(decoder);stb_vorbis_close(decoder);
    } else if(!std::memcmp(header,"RIFF",4) && !std::memcmp(header+8,"WAVE",4)) {
        drwav decoder{};if(!drwav_init_file(&decoder,path,nullptr))return false;
        rate=decoder.sampleRate;channels=decoder.channels;frames=decoder.totalPCMFrameCount;drwav_uninit(&decoder);
    } else {
        drmp3 decoder{};if(!drmp3_init_file(&decoder,path,nullptr))return false;
        rate=decoder.sampleRate;channels=decoder.channels;frames=drmp3_get_pcm_frame_count(&decoder);drmp3_uninit(&decoder);
    }
    if(rate<8000 || rate>192000 || (channels!=1 && channels!=2) || !frames || frames>MaxSongFrames*rate/48000)return false;
    info={(frames*48000+rate-1)/rate,rate,channels,false,frames};return true;
}
bool decodeArchiveRange(const char* path,const AudioInfo& info,uint64_t first,uint64_t count,
                        int16_t*& pcm,uint64_t& frames,const std::atomic<bool>* cancel=nullptr) noexcept {
    const uint64_t sourceFirst=first*info.rate/48000;
    const uint64_t seek=info.rate==48000?first:(sourceFirst?sourceFirst-1:0);
    Resample48 resample{pcm,frames,count,0,info.rate,info.channels};
    resample.sourceFrames=seek;resample.next=first*info.rate;resample.stopAfter=count;
    std::array<int16_t,8192> data{};bool ok=true;
    FILE* f=std::fopen(path,"rb");if(!f)return false;uint8_t magic[12]{};
    ok=readExact(f,magic,sizeof(magic));std::fclose(f);if(!ok)return false;
    if(!std::memcmp(magic,"OggS",4)) {
        int error=0;auto* decoder=stb_vorbis_open_filename(path,&error,nullptr);if(!decoder)return false;
        ok=stb_vorbis_seek(decoder,static_cast<unsigned>(seek))!=0;
        while(ok && !resample.done() && (!cancel || !cancel->load(std::memory_order_relaxed))) {const int n=stb_vorbis_get_samples_short_interleaved(decoder,info.channels,data.data(),int(data.size()));if(n<=0)break;ok=resample.feed(data.data(),size_t(n));}
        ok=ok && stb_vorbis_get_error(decoder)==0;stb_vorbis_close(decoder);
    } else if(!std::memcmp(magic,"RIFF",4)) {
        drwav decoder{};if(!drwav_init_file(&decoder,path,nullptr))return false;
        ok=drwav_seek_to_pcm_frame(&decoder,seek)!=0;
        while(ok && !resample.done() && (!cancel || !cancel->load(std::memory_order_relaxed))){const auto n=drwav_read_pcm_frames_s16(&decoder,data.size()/info.channels,data.data());if(!n)break;ok=resample.feed(data.data(),size_t(n));}
        drwav_uninit(&decoder);
    } else {
        drmp3 decoder{};if(!drmp3_init_file(&decoder,path,nullptr))return false;
        ok=drmp3_seek_to_pcm_frame(&decoder,seek)!=0;
        while(ok && !resample.done() && (!cancel || !cancel->load(std::memory_order_relaxed))){const auto n=drmp3_read_pcm_frames_s16(&decoder,data.size()/info.channels,data.data());if(!n)break;ok=resample.feed(data.data(),size_t(n));}
        drmp3_uninit(&decoder);
    }
    return ok && resample.finish() && frames==count;
}

bool decodeOggOpus(FILE* file, int16_t*& pcm, uint64_t& frames, uint64_t frameLimit,uint64_t first=0,uint64_t rangeCount=0,const std::atomic<bool>* cancel=nullptr) noexcept {
    const bool ranged=rangeCount!=0;bool rangeComplete=false;
    frameLimit = std::min(frameLimit, MaxSongFrames);
    if (frameLimit == 0) return false;
    uint8_t header[27];
    uint8_t lacing[255];
    uint8_t* packet = nullptr;
    size_t packetLength = 0, packetCapacity = 0;
    uint64_t capacity = 0, finalGranule = UINT64_MAX,storedFrames=0;
    uint64_t decodedFrames = 0;
    uint64_t encodedBytes = 0;
    uint32_t serial = 0;
    bool haveSerial = false, streamEnded = false, cleanEof = false;
    int channels = 0, preSkip = 0, headerPackets = 0;
    OpusDecoder* decoder = nullptr;
    bool ok = true;
    int16_t decoded[MaxDecodedFrames * 2];

    while (ok && !streamEnded && !rangeComplete) {
        if(cancel && cancel->load(std::memory_order_relaxed)){ok=false;break;}
        const size_t got = std::fread(header, 1, sizeof(header), file);
        if (got == 0) {cleanEof=!std::ferror(file);break;}
        if (got != sizeof(header) || std::memcmp(header, "OggS", 4) != 0 || header[4] != 0) {
            ok = false;
            break;
        }
        const uint8_t segmentCount = header[26];
        if (!readExact(file, lacing, segmentCount)) { ok = false; break; }
        size_t pageBytes = 0;
        for (unsigned i = 0; i < segmentCount; ++i) pageBytes += lacing[i];
        const uint64_t pageSize = sizeof(header) + segmentCount + pageBytes;
        if (pageBytes > 65025 || pageSize > MaxEncodedBytes - encodedBytes) { ok = false; break; }
        encodedBytes += pageSize;
        uint8_t* pagePayload = static_cast<uint8_t*>(std::malloc(pageBytes ? pageBytes : 1));
        if (!pagePayload) { ok = false; break; }
        if (!readExact(file, pagePayload, pageBytes)) {
            std::free(pagePayload);
            ok = false;
            break;
        }

        const uint32_t pageSerial = readU32(header + 14);
        if (!haveSerial) {
            if ((header[5] & 0x02u) == 0) {
                std::free(pagePayload);
                ok = false;
                break;
            }
            serial = pageSerial;
            haveSerial = true;
        }
        if (pageSerial != serial) {
            std::free(pagePayload);
            continue;
        }

        size_t offset = 0;
        for (unsigned segment = 0; ok && segment < segmentCount; ++segment) {
            const size_t part = lacing[segment];
            if (part > MaxPacketBytes - packetLength) { ok = false; break; }
            const size_t required = packetLength + part;
            if (required > packetCapacity) {
                size_t grown = packetCapacity ? packetCapacity : 4096;
                while (grown < required && grown < MaxPacketBytes) grown = std::min(grown * 2, MaxPacketBytes);
                void* allocation = std::realloc(packet, grown);
                if (!allocation) { ok = false; break; }
                packet = static_cast<uint8_t*>(allocation);
                packetCapacity = grown;
            }
            if (part) std::memcpy(packet + packetLength, pagePayload + offset, part);
            packetLength += part;
            offset += part;
            if (part == 255) continue;

            if (headerPackets == 0) {
                if (packetLength < 19 || std::memcmp(packet, "OpusHead", 8) != 0 || packet[8] > 15) {
                    ok = false;
                } else {
                    channels = packet[9];
                    preSkip = int(packet[10]) | (int(packet[11]) << 8);
                    if ((channels != 1 && channels != 2) || packet[18] != 0) {
                        ok = false; // This title supports the standard mono/stereo mapping.
                    } else {
                        int error = OPUS_OK;
                        decoder = opus_decoder_create(48000, channels, &error);
                        ok = decoder != nullptr && error == OPUS_OK;
                    }
                }
                ++headerPackets;
            } else if (headerPackets == 1) {
                if (packetLength < 8 || std::memcmp(packet, "OpusTags", 8) != 0) ok = false;
                ++headerPackets;
            } else if (ok) {
                const int samples=opus_packet_get_nb_samples(packet,static_cast<opus_int32>(packetLength),48000);
                const uint64_t wanted=first+uint64_t(preSkip),end=wanted+rangeCount;
                // RFC7845 requires at least80ms; retain1s of decoder history
                // to suppress small convergence differences at the clip boundary.
                const bool decode=!ranged || decodedFrames+uint64_t(std::max(0,samples))+48000>wanted;
                if(samples<=0 || samples>MaxDecodedFrames)ok=false;
                else if(decode) {
                    const int count=opus_decode(decoder,packet,static_cast<opus_int32>(packetLength),decoded,MaxDecodedFrames,0);
                    if(count!=samples)ok=false;
                    else if(ranged) {
                        const auto from=std::max(decodedFrames,wanted),to=std::min(decodedFrames+uint64_t(count),end);
                        if(to>from && !appendFrames(pcm,storedFrames,capacity,decoded+(from-decodedFrames)*channels,int(to-from),channels,frameLimit))ok=false;
                    } else if(!appendFrames(pcm,storedFrames,capacity,decoded,count,channels,frameLimit))ok=false;
                }
                if(samples>0)decodedFrames+=uint64_t(samples);
                if(ranged && decodedFrames>=end)rangeComplete=true;
            }
            packetLength = 0;
            if(rangeComplete)break;
        }
        // Some PC chart audio (including the bundled Afterlife) omits EOS.
        // Only a complete final page/packet and a usable granule can supply
        // its duration. Partial pages, pending packets and decode errors fail.
        finalGranule=readU64(header+6);
        if ((header[5] & 0x04u) != 0) streamEnded = true;
        std::free(pagePayload);
    }

    if(ranged) {
        ok=ok && rangeComplete && storedFrames==rangeCount && headerPackets>=2 && decoder;
        frames=ok?storedFrames:0;
    } else {
    if ((!streamEnded && !cleanEof) || packetLength!=0 || headerPackets < 2 || !decoder || finalGranule == UINT64_MAX ||
        finalGranule < static_cast<uint64_t>(preSkip) || decodedFrames < static_cast<uint64_t>(preSkip))
        ok = false;
    if (ok) {
        const uint64_t outputFrames = finalGranule - static_cast<uint64_t>(preSkip);
        if (outputFrames > decodedFrames - static_cast<uint64_t>(preSkip) || outputFrames > frameLimit)
            ok = false;
        else {
            std::memmove(pcm, pcm + static_cast<uint64_t>(preSkip) * 2,
                        static_cast<size_t>(outputFrames) * 2 * sizeof(int16_t));
            frames = outputFrames;
        }
    }
    }
    if (decoder) opus_decoder_destroy(decoder);
    std::free(packet);
    return ok;
}
} // namespace

AudioPlayer::~AudioPlayer() { clear(); }

bool AudioPlayer::loadAudioFile(const char* path, uint64_t maxDecodedBytes) noexcept {
    clear();
    if (!path || !*path) return false;
    FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    int16_t* decoded = nullptr;
    uint64_t frames = 0;
    const uint64_t maxFrames = std::min<uint64_t>(MaxSongFrames,
        maxDecodedBytes / (2 * sizeof(int16_t)));
    bool ok = decodeOggOpus(file, decoded, frames, maxFrames);
    std::fclose(file);
    if(!ok){std::free(decoded);decoded=nullptr;frames=0;ok=decodeArchiveAudio(path,decoded,frames,maxFrames);}
    if (!ok || !decoded || frames == 0) {
        std::free(decoded);
        return false;
    }
    // Drop geometric decoder growth slack, especially for short UI cues.
    // This runs before publication, never on the hardware callback.
    if(void* tight=std::realloc(decoded,size_t(frames)*2*sizeof(int16_t)))decoded=static_cast<int16_t*>(tight);
    pcm_ = decoded;
    frameCount_ = frames;totalFrames_=frames;originFrames_=0;
    playbackEndFrames_.store(frames,std::memory_order_release);
    cursor_.store(0, std::memory_order_release);
    playbackStep_.store(AudioPhaseOne, std::memory_order_release);
    return true;
}

double AudioPlayer::probeDuration(const char* path) noexcept {
    AudioInfo info;return path && *path && probeAudio(path,info)?double(info.frames48)/48000.0:0;
}
bool AudioPlayer::loadAudioRange(const char* path,double start,double length,uint64_t maxDecodedBytes,const std::atomic<bool>* cancel) noexcept {
    clear();AudioInfo info;
    if(cancel && cancel->load(std::memory_order_acquire))return false;
    if(!path || !*path || !std::isfinite(start) || !std::isfinite(length) || start<0 || length<=0 || !probeAudio(path,info))return false;
    const auto first=uint64_t(std::round(std::min(start,double(info.frames48)/48000.0)*48000.0));
    const auto count=std::min(info.frames48-first,uint64_t(std::round(std::min(length,3600.0)*48000.0)));
    if(!count || count>maxDecodedBytes/(2*sizeof(int16_t)))return false;
    int16_t* decoded=nullptr;uint64_t frames=0;bool ok=false;
    if(info.opus) {
        if(FILE* file=std::fopen(path,"rb")){ok=decodeOggOpus(file,decoded,frames,count,first,count,cancel);std::fclose(file);}
    } else ok=decodeArchiveRange(path,info,first,count,decoded,frames,cancel);
    if(!ok || !decoded || frames!=count){std::free(decoded);return false;}
    pcm_=decoded;frameCount_=frames;originFrames_=first;totalFrames_=info.frames48;
    playbackEndFrames_.store(frames,std::memory_order_release);cursor_.store(0,std::memory_order_release);
    playbackStep_.store(AudioPhaseOne,std::memory_order_release);return true;
}

void AudioPlayer::clear() noexcept {
    pause();
    std::free(pcm_);
    pcm_ = nullptr;
    frameCount_ = originFrames_ = totalFrames_ = 0;
    playbackEndFrames_.store(0,std::memory_order_release);
    cursor_.store(0, std::memory_order_release);
    playbackStep_.store(AudioPhaseOne, std::memory_order_release);
}

void AudioPlayer::takeDecodedFrom(AudioPlayer& source) noexcept {
    if (this == &source) return;
    clear();
    pcm_ = source.pcm_;
    frameCount_ = source.frameCount_;originFrames_=source.originFrames_;totalFrames_=source.totalFrames_;
    playbackEndFrames_.store(source.playbackEndFrames_.load(std::memory_order_acquire),std::memory_order_release);
    cursor_.store(source.cursor_.load(std::memory_order_acquire), std::memory_order_release);
    playbackStep_.store(source.playbackStep_.load(std::memory_order_acquire), std::memory_order_release);
    gain_.store(source.gain_.load(std::memory_order_acquire), std::memory_order_release);
    playing_.store(false, std::memory_order_release);
    source.pcm_ = nullptr;
    source.frameCount_ = source.originFrames_ = source.totalFrames_ = 0;
    source.cursor_.store(0, std::memory_order_release);
    source.playing_.store(false, std::memory_order_release);
}

void AudioPlayer::rewind() noexcept {
    pause();
    playbackEndFrames_.store(frameCount_,std::memory_order_release);
    cursor_.store(0, std::memory_order_release);
}

void AudioPlayer::seekSeconds(double seconds) noexcept {
    pause();
    playbackEndFrames_.store(frameCount_,std::memory_order_release);
    if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
    const double requestedFrames = std::max(0.0,seconds*48000.0-double(originFrames_));
    if (requestedFrames >= static_cast<double>(frameCount_)) {
        cursor_.store(frameCount_ * AudioPhaseOne, std::memory_order_release);
        return;
    }
    const uint64_t wholeFrames = static_cast<uint64_t>(requestedFrames);
    const double fraction = requestedFrames - static_cast<double>(wholeFrames);
    const uint64_t fractionalPhase = static_cast<uint64_t>(fraction * static_cast<double>(AudioPhaseOne));
    cursor_.store((wholeFrames * AudioPhaseOne) + fractionalPhase, std::memory_order_release);
}

void AudioPlayer::setPlaybackWindow(double start,double length) noexcept {
    pause();
    if(!std::isfinite(start) || start<0) start=0;
    if(!std::isfinite(length) || length<0) length=0;
    const double offset=std::clamp(start*48000.0-double(originFrames_),0.0,double(frameCount_));
    const uint64_t first=static_cast<uint64_t>(std::round(offset));
    const uint64_t count=static_cast<uint64_t>(std::round(std::min(length,durationSeconds())*48000.0));
    cursor_.store(first*AudioPhaseOne,std::memory_order_release);
    playbackEndFrames_.store(first+std::min(count,frameCount_-first),std::memory_order_release);
}

void AudioPlayer::setPlaybackRate(float rate) noexcept {
    if (!std::isfinite(rate)) rate = 1.0f;
    rate = std::clamp(rate, 0.25f, 2.0f);
    const auto step = static_cast<uint64_t>(static_cast<double>(rate) * static_cast<double>(AudioPhaseOne));
    playbackStep_.store(std::max<uint64_t>(step, 1), std::memory_order_release);
}

bool AudioPlayer::play() noexcept {
    if (!ready() || (cursor_.load(std::memory_order_acquire) >> 32) >= playbackEndFrames_.load(std::memory_order_acquire)) return false;
    playing_.store(true, std::memory_order_release);
    return true;
}

void AudioPlayer::pause() noexcept { playing_.store(false, std::memory_order_release); }

double AudioPlayer::positionSeconds() const noexcept {
    return double(originFrames_)/48000.0+static_cast<double>(cursor_.load(std::memory_order_acquire)) /
           (static_cast<double>(AudioPhaseOne) * 48000.0);
}

double AudioPlayer::durationSeconds() const noexcept { return static_cast<double>(totalFrames_) / 48000.0; }

void AudioPlayer::setGain(float gain) noexcept {
    if (!std::isfinite(gain)) gain = 0.0f;
    gain_.store(std::clamp(gain, 0.0f, 2.0f), std::memory_order_release);
}

void AudioPlayer::audioFill(int16_t* output, int frames, void* user) noexcept {
    if (!output || frames <= 0) return;
    if (!user) {
        std::memset(output, 0, static_cast<size_t>(frames) * 2 * sizeof(int16_t));
        return;
    }
    static_cast<AudioPlayer*>(user)->fill(output, frames);
}

void AudioPlayer::fill(int16_t* output, int frames) noexcept {
    if (!playing_.load(std::memory_order_acquire) || !pcm_) {
        std::memset(output, 0, static_cast<size_t>(frames) * 2 * sizeof(int16_t));
        return;
    }
    uint64_t phase = cursor_.load(std::memory_order_relaxed);
    const uint64_t phaseEnd = playbackEndFrames_.load(std::memory_order_acquire) * AudioPhaseOne;
    const uint64_t step = playbackStep_.load(std::memory_order_acquire);
    const float gain = gain_.load(std::memory_order_acquire);
    int written = 0;
    while (written < frames && phase < phaseEnd) {
        const uint64_t sourceFrame = phase >> 32;
        const uint64_t nextFrame = std::min(sourceFrame + 1, frameCount_ - 1);
        const float fraction = static_cast<float>(phase & 0xFFFFFFFFu) / 4294967296.0f;
        for (unsigned channel = 0; channel < 2; ++channel) {
            const float first = static_cast<float>(pcm_[sourceFrame * 2 + channel]);
            const float second = static_cast<float>(pcm_[nextFrame * 2 + channel]);
            const float sample = (first + (second - first) * fraction) * gain;
            output[static_cast<size_t>(written) * 2 + channel] = static_cast<int16_t>(
                std::clamp(sample, -32768.0f, 32767.0f));
        }
        ++written;
        phase = step > phaseEnd - phase ? phaseEnd : phase + step;
    }
    if (written < frames)
        std::memset(output + static_cast<size_t>(written) * 2, 0,
                    static_cast<size_t>(frames - written) * 2 * sizeof(int16_t));
    cursor_.store(phase, std::memory_order_release);
    if (phase >= phaseEnd) playing_.store(false, std::memory_order_release);
}

#include "audio_stream.inc"

AudioStemMixer::~AudioStemMixer(){clear();}
void AudioStemMixer::sourceUnavailable() noexcept {pause();if(stream_)stream_->sourceUnavailable();}
bool AudioStemMixer::streamFailed() const noexcept {return stream_ && stream_->failed();}
bool AudioStemMixer::streamBuffering() const noexcept {return stream_ && stream_->buffering();}
uint64_t AudioStemMixer::streamUnderruns() const noexcept {return stream_?stream_->underruns():0;}

bool AudioStemMixer::loadPreviewStream(const char* chartPath, const char* musicStream) noexcept {
    clear();
    if (!chartPath || !*chartPath) return false;
    const char* slash = std::strrchr(chartPath, '/');
    if (!slash) return false;
    const std::string folder(chartPath,static_cast<size_t>(slash-chartPath));
    const auto loadDedicated=[&](const std::string& path) {
        if(!players_[12].loadAudioFile(path.c_str())) return false;
        loaded_[12]=true;
        loadedCount_=1;
        dedicatedPreview_=true;
        return true;
    };
    static constexpr const char* names[]={"preview","song_preview"};
    static constexpr const char* extensions[]={".opus",".ogg",".mp3",".wav"};
    for(const char* name:names) for(const char* extension:extensions)
        if(loadDedicated(folder+"/"+name+extension)) return true;
    // Some original chart packages capitalize their preview filenames.
    if(DIR* directory=opendir(folder.c_str())) {
        bool loaded=false;
        while(dirent* entry=readdir(directory)) {
            std::string lower=entry->d_name;
            for(char& c:lower) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            bool preview=false;
            for(const char* name:names) for(const char* extension:extensions)
                if(lower==std::string(name)+extension) preview=true;
            if(preview && loadDedicated(folder+"/"+entry->d_name)) { loaded=true; break; }
        }
        closedir(directory);
        if(loaded) return true;
    }
    // PC's stem matrix supplies the full mix, never just the first playable track.
    return loadChartStems(chartPath,musicStream);
}

bool AudioStemMixer::loadPreviewClip(const char* chartPath,const char* musicStream,bool mainMenu,
                                        double authoredStart,double randomUnit,const std::atomic<bool>* cancel) noexcept {
    clear();if(!chartPath || !*chartPath)return false;
    const char* slash=std::strrchr(chartPath,'/');if(!slash)return false;
    const std::string folder(chartPath,size_t(slash-chartPath));
    if(DIR* directory=opendir(folder.c_str())) {
        bool loaded=false;
        while(auto* entry=readdir(directory)) {
            std::string lower=entry->d_name;
            for(char& c:lower)c=char(std::tolower(static_cast<unsigned char>(c)));
            bool dedicated=false;
            for(const char* name:{"preview","song_preview"})for(const char* ext:{".opus",".ogg",".mp3",".wav"})
                if(lower==std::string(name)+ext)dedicated=true;
            if(!dedicated)continue;
            const auto path=folder+"/"+entry->d_name;
            const double duration=AudioPlayer::probeDuration(path.c_str());
            const double length=std::min(mainMenu?60.0:30.0,duration);
            if(length<=0 || !players_[12].loadAudioRange(path.c_str(),0,length,uint64_t{48000}*60*4,cancel))continue;
            loaded_[12]=true;loadedCount_=1;dedicatedPreview_=previewClip_=true;
            previewStart_=0;previewLength_=length;previewFadeIn_=mainMenu?std::min(1.0,length):0;loaded=true;break;
        }
        closedir(directory);if(loaded)return true;
    }
    const PreviewRequest request{mainMenu,authoredStart,randomUnit};
    return loadStems(chartPath,musicStream,&request,false,cancel);
}
uint64_t AudioStemMixer::pcmBytes() const noexcept {
    uint64_t bytes=stream_?stream_->pcmBytes():0;for(size_t i=0;i<StemCount;++i)if(loaded_[i])bytes+=players_[i].pcmBytes();return bytes;
}

bool AudioStemMixer::loadChartStems(const char* chartPath,const char* musicStream) noexcept {
    return loadStems(chartPath,musicStream,nullptr);
}
bool AudioStemMixer::loadGameplayStream(const char* chartPath,const char* musicStream,const std::atomic<bool>* cancel) noexcept {
    return loadStems(chartPath,musicStream,nullptr,true,cancel);
}
bool AudioStemMixer::loadStems(const char* chartPath,const char* musicStream,const PreviewRequest* preview,bool streaming,const std::atomic<bool>* cancel) noexcept {
    clear();
    if (!chartPath || !*chartPath) return false;
    static constexpr const char* names[StemCount] = {
        "guitar", "bass", "rhythm", "vocals", "vocals_1", "vocals_2",
        "drums", "drums_1", "drums_2", "drums_3", "drums_4", "keys", "song", "crowd"
    };
    static constexpr const char* containers[] = { ".opus", ".ogg", ".mp3", ".wav" };
    static constexpr uint64_t MaxStemPcmBytes = uint64_t{768} * 1024 * 1024;
    const char* slash = std::strrchr(chartPath, '/');
    if (!slash) return false;
    const size_t folderLength = static_cast<size_t>(slash - chartPath);
    const std::string folder(chartPath,folderLength);
    std::vector<std::string> files;
    if(DIR* directory=opendir(folder.c_str())) {
        while(dirent* entry=readdir(directory))if(entry->d_name[0]!='.')files.emplace_back(entry->d_name);
        closedir(directory);
    }
    const auto lower=[](std::string name){for(char& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return name;};
    const auto fileFor=[&](const std::string& name) {
        const std::string key=lower(name);
        for(const auto& file:files)if(lower(file)==key)return folder+"/"+file;
        // Direct-read fallback is important for bundled PS5 asset mounts.
        return files.empty() || name.find('/')!=std::string::npos ? folder+"/"+name : std::string{};
    };
    if(preview) {
        double duration=0;
        for(const char* stem:names)for(const char* container:containers) {
            const auto candidate=fileFor(std::string(stem)+container);
            if(!candidate.empty())duration=std::max(duration,AudioPlayer::probeDuration(candidate.c_str()));
        }
        if(musicStream && *musicStream && musicStream[0]!='/' && !std::strstr(musicStream,"..") && !std::strchr(musicStream,'\\')) {
            const auto candidate=fileFor(musicStream);
            if(!candidate.empty())duration=std::max(duration,AudioPlayer::probeDuration(candidate.c_str()));
        }
        auto window=preview->menu?menuPreviewWindow(duration,false,preview->randomUnit):
            PreviewWindow{std::clamp(preview->authoredStart,0.0,std::max(0.0,duration-std::min(30.0,duration))),std::min(30.0,duration)};
        previewStart_=std::round(window.start*48000.0)/48000.0;previewLength_=window.length;previewClip_=true;
        previewFadeOut_=preview->menu?std::min(5.0,previewLength_):0;
        previewFadeIn_=preview->menu?std::min(1.0,previewLength_):0;
        if(previewLength_<=0)return false;
    }
    std::array<std::string,StemCount> streamPaths{};
    const auto load=[&](size_t stem,const char* path,uint64_t budget) {
        if(cancel && cancel->load(std::memory_order_acquire))return false;
        if(streaming){if(AudioPlayer::probeDuration(path)<=0)return false;streamPaths[stem]=path;return true;}
        return preview?players_[stem].loadAudioRange(path,previewStart_,previewLength_,budget,cancel):
            players_[stem].loadAudioFile(path,budget);
    };
    char path[4096];
    uint64_t totalPcmBytes = 0;
    for (size_t stem = 0; stem < StemCount; ++stem) {
        for (size_t container = 0; container < std::size(containers); ++container) {
            // The PC stem/container matrix also covers original ZIP audio.
            const std::string candidate=fileFor(std::string(names[stem])+containers[container]);
            if(candidate.empty())continue;
            const int length = std::snprintf(path,sizeof(path),"%s",candidate.c_str());
            if (length <= 0 || static_cast<size_t>(length) >= sizeof(path)) continue;
            const uint64_t remaining = MaxStemPcmBytes - std::min(totalPcmBytes, MaxStemPcmBytes);
            if (!load(stem,path,remaining)) continue;
            const uint64_t bytes = players_[stem].pcmBytes();
            if (bytes > MaxStemPcmBytes - std::min(totalPcmBytes, MaxStemPcmBytes)) {
                players_[stem].clear();
                continue;
            }
            totalPcmBytes += bytes;
            loaded_[stem] = true;
            ++loadedCount_;
            break;
        }
    }
    bool musicStreamIsNamedStem = false;
    if (musicStream && *musicStream) {
        for (size_t stem = 0; stem < StemCount; ++stem)
            for (const char* container : containers) {
                char expected[64];
                const int length = std::snprintf(expected, sizeof(expected), "%s%s", names[stem], container);
                if (length > 0 && static_cast<size_t>(length) < sizeof(expected) &&
                    lower(musicStream)==expected)
                    musicStreamIsNamedStem = true;
            }
    }
    if (!musicStreamIsNamedStem && musicStream && *musicStream && musicStream[0] != '/' &&
        std::strstr(musicStream, "..") == nullptr && std::strchr(musicStream, '\\') == nullptr) {
        const std::string candidate=fileFor(musicStream);
        const int length = candidate.empty()?0:std::snprintf(path,sizeof(path),"%s",candidate.c_str());
        if (length > 0 && static_cast<size_t>(length) < sizeof(path)) {
            // A chart's generic MusicStream belongs to the PC `song` channel
            // when that conventional stem is absent. Otherwise use the first
            // unused PC slot without replacing a decoded chart stem.
            for (size_t attempt = 0; attempt < StemCount; ++attempt) {
                const size_t stem = attempt == 0 ? 12 : (attempt <= 12 ? attempt - 1 : attempt);
                if (loaded_[stem]) continue;
                const uint64_t remaining = MaxStemPcmBytes - std::min(totalPcmBytes, MaxStemPcmBytes);
                if (load(stem,path,remaining)) {
                    const uint64_t bytes = players_[stem].pcmBytes();
                    if (bytes <= MaxStemPcmBytes - std::min(totalPcmBytes, MaxStemPcmBytes)) {
                        totalPcmBytes += bytes;
                        loaded_[stem] = true;
                        ++loadedCount_;
                    } else {
                        players_[stem].clear();
                    }
                }
                break;
            }
        }
    }
    if(streaming && loadedCount_){stream_=StreamedSong::create(streamPaths,cancel);if(!stream_)loadedCount_=0;}
    if (loadedCount_ == 0) clear();
    return loadedCount_ != 0;
}

void AudioStemMixer::takeChartStemsFrom(AudioStemMixer& source) noexcept {
    if (this == &source) return;
    clear();
    stream_=source.stream_;source.stream_=nullptr;
    for (size_t i = 0; i < StemCount; ++i) {
        players_[i].takeDecodedFrom(source.players_[i]);
        loaded_[i] = source.loaded_[i];
        source.loaded_[i] = false;
    }
    dedicatedPreview_=source.dedicatedPreview_;previewClip_=source.previewClip_;
    previewStart_=source.previewStart_;previewLength_=source.previewLength_;previewFadeOut_=source.previewFadeOut_;previewFadeIn_=source.previewFadeIn_;
    source.dedicatedPreview_=source.previewClip_=false;source.previewStart_=source.previewLength_=source.previewFadeOut_=source.previewFadeIn_=0;
    loadedCount_ = source.loadedCount_;
    source.loadedCount_ = 0;
}

bool AudioStemMixer::loadUiSounds(const char* directory,const char* introPath) noexcept {
    if (!directory || !*directory) return false;
    static constexpr const char* names[] = {
        "UIConfirm.opus", "UICancel.opus", "UIScrollUp.opus", "UIScrollDown.opus",
        "bad_note1.opus", "bad_note2.opus", "bad_note3.opus", "bad_note4.opus",
        "bad_note5.opus", "ui_lose_multiplier.opus", "ghost_decrease.opus",
        "notes_ripple_up_01.opus", "sp_awarded.opus", "star_available.opus",
        "star_deployed.opus", "star_release.opus", "ui_rewind_highway_sound.opus", "menu_intro.wav"
    };
    bool anyLoaded = false;
    char path[1024];
    for (size_t i = 0; i < uiPlayers_.size(); ++i) {
        uiPlayers_[i].clear();
        uiLoaded_[i] = false;
        const int length = introPath && i==size_t(UiSound::MenuIntro)?std::snprintf(path,sizeof(path),"%s",introPath):std::snprintf(path, sizeof(path), "%s%s%s", directory,
            directory[std::strlen(directory) - 1] == '/' ? "" : "/", names[i]);
        if (length <= 0 || static_cast<size_t>(length) >= sizeof(path)) continue;
        uiLoaded_[i] = uiPlayers_[i].loadAudioFile(path, uint64_t{48000} * (i==size_t(UiSound::MenuIntro)?30:5) * 2 * sizeof(int16_t));
        if (uiLoaded_[i]) uiPlayers_[i].setGain(uiGain_ * (i>=size_t(UiSound::BadNote1) && i<=size_t(UiSound::BadNote5)?missSoundGain_:1.0f));
        anyLoaded |= uiLoaded_[i];
    }
    return anyLoaded;
}

void AudioStemMixer::playUiSound(UiSound sound) noexcept {
    const size_t index = static_cast<size_t>(sound);
    if (index >= uiPlayers_.size() || !uiLoaded_[index]) return;
    uiPlayers_[index].rewind();
    (void)uiPlayers_[index].play();
}

void AudioStemMixer::setUiGain(float gain) noexcept {
    if (!std::isfinite(gain)) gain = 0.0f;
    uiGain_ = std::clamp(gain, 0.0f, 2.0f);
    for (size_t i = 0; i < uiPlayers_.size(); ++i)
        if (uiLoaded_[i]) uiPlayers_[i].setGain(uiGain_ * (i>=size_t(UiSound::BadNote1) && i<=size_t(UiSound::BadNote5)?missSoundGain_:1.0f));
}

void AudioStemMixer::setMissSoundGain(float gain) noexcept {
    missSoundGain_=std::isfinite(gain)?std::clamp(gain,0.0f,1.0f):0.0f;
    for(size_t i=size_t(UiSound::BadNote1);i<=size_t(UiSound::BadNote5);++i)
        if(uiLoaded_[i])uiPlayers_[i].setGain(uiGain_*missSoundGain_);
}

void AudioStemMixer::clear() noexcept {
    delete stream_;stream_=nullptr;
    for (size_t i = 0; i < StemCount; ++i) {
        players_[i].clear();
        loaded_[i] = false;
    }
    loadedCount_ = 0;
    dedicatedPreview_=previewClip_=false;previewStart_=previewLength_=previewFadeOut_=previewFadeIn_=0;
}

void AudioStemMixer::seekSeconds(double seconds) noexcept {
    if(stream_){stream_->seek(seconds);return;}
    for (size_t i = 0; i < StemCount; ++i)
        if (loaded_[i]) players_[i].seekSeconds(seconds);
}

void AudioStemMixer::setPlaybackWindow(double start,double length) noexcept {
    if(stream_){stream_->window(start,length);return;}
    for(size_t i=0;i<StemCount;++i)
        if(loaded_[i]) players_[i].setPlaybackWindow(start,length);
}

void AudioStemMixer::setPlaybackRate(float rate) noexcept {
    if(stream_)stream_->rate(rate);
    for (size_t i = 0; i < StemCount; ++i)
        if (loaded_[i]) players_[i].setPlaybackRate(rate);
}

bool AudioStemMixer::play() noexcept {
    if(stream_)return stream_->play();
    bool started = false;
    for (size_t i = 0; i < StemCount; ++i)
        if (loaded_[i]) started = players_[i].play() || started;
    return started;
}

void AudioStemMixer::pause() noexcept {
    if(stream_)stream_->pause();
    for (size_t i = 0; i < StemCount; ++i)
        if (loaded_[i]) players_[i].pause();
}

bool AudioStemMixer::playing() const noexcept {
    if(stream_)return stream_->playing();
    for (size_t i = 0; i < StemCount; ++i)
        if (loaded_[i] && players_[i].playing()) return true;
    return false;
}

double AudioStemMixer::positionSeconds() const noexcept {
    if(stream_)return stream_->position();
    size_t clockStem = StemCount;
    double longest = 0.0;
    for (size_t i = 0; i < StemCount; ++i) {
        if (!loaded_[i]) continue;
        const double duration = players_[i].durationSeconds();
        if (duration > longest) { longest = duration; clockStem = i; }
    }
    return clockStem < StemCount ? players_[clockStem].positionSeconds() : 0.0;
}

double AudioStemMixer::durationSeconds() const noexcept {
    if(stream_)return stream_->duration();
    double duration = 0.0;
    for (size_t i = 0; i < StemCount; ++i)
        if (loaded_[i]) duration = std::max(duration, players_[i].durationSeconds());
    return duration;
}

void AudioStemMixer::setGain(float gain) noexcept {
    if (!std::isfinite(gain)) gain = 0.0f;
    masterGain_ = std::clamp(gain, 0.0f, 2.0f);
    if(stream_)for(size_t i=0;i<StemCount;++i)stream_->gain(i,masterGain_*stemGains_[i]);
    for (size_t i = 0; i < StemCount; ++i)
        if (loaded_[i]) players_[i].setGain(masterGain_ * stemGains_[i]);
}

void AudioStemMixer::setStemGain(size_t stem, float gain) noexcept {
    if (stem >= StemCount) return;
    if (!std::isfinite(gain)) gain = 0.0f;
    stemGains_[stem] = std::clamp(gain, 0.0f, 2.0f);
    if(stream_)stream_->gain(stem,masterGain_*stemGains_[stem]);
    if (loaded_[stem]) players_[stem].setGain(masterGain_ * stemGains_[stem]);
}

void AudioStemMixer::audioFill(int16_t* output, int frames, void* user) noexcept {
    if (!output || frames <= 0) return;
    auto* mixer = static_cast<AudioStemMixer*>(user);
    if (!mixer) {
        std::memset(output, 0, static_cast<size_t>(frames) * 2 * sizeof(int16_t));
        return;
    }
    if(mixer->calibrationTone.active()) {
        timespec now{};clock_gettime(CLOCK_MONOTONIC,&now);
        mixer->calibrationTone.fill(output,unsigned(frames),uint64_t(now.tv_sec)*1000000+uint64_t(now.tv_nsec)/1000);
        return;
    }
    for (int offset = 0; offset < frames; offset += 256) {
        const int count = std::min(256, frames - offset);
        // The preview transport uses 48-kHz sample time, independent of UI FPS.
        // Scale the full stem sum before adding interface effects.
        const auto fadeInFrames=static_cast<int64_t>(std::llround(mixer->previewFadeIn_*48000.0));
        const double previewPosition=(fadeInFrames>0 || mixer->previewFadeOut_>0)?mixer->positionSeconds():0.0;
        const auto elapsed=fadeInFrames>0?static_cast<int64_t>(std::llround(std::max(0.0,previewPosition-mixer->previewStart_)*48000.0)):0;
        const auto fadeFrames=static_cast<int64_t>(std::llround(mixer->previewFadeOut_*48000.0));
        const auto remaining=fadeFrames>0?static_cast<int64_t>(std::llround(std::max(0.0,
            mixer->previewStart_+mixer->previewLength_-previewPosition)*48000.0)):0;
        std::fill_n(mixer->accumulator_.data(), static_cast<size_t>(count) * 2, 0);
        if(mixer->stream_)mixer->stream_->mix(mixer->accumulator_.data(),count);
        for (size_t stem = 0; !mixer->stream_ && stem < StemCount; ++stem) {
            if (!mixer->loaded_[stem] || !mixer->players_[stem].playing()) continue;
            AudioPlayer::audioFill(mixer->scratch_.data(), count, &mixer->players_[stem]);
            for (int sample = 0; sample < count * 2; ++sample)
                mixer->accumulator_[static_cast<size_t>(sample)] += mixer->scratch_[static_cast<size_t>(sample)];
        }
        if(fadeFrames>0 || fadeInFrames>0)for(int frame=0;frame<count;++frame) {
            const double in=fadeInFrames>0?std::clamp(double(elapsed+frame)/double(fadeInFrames),0.0,1.0):1.0;
            const double out=fadeFrames>0?std::clamp(double(remaining-frame)/double(fadeFrames),0.0,1.0):1.0;
            const float gain=static_cast<float>(std::min(in,out));
            for(unsigned channel=0;channel<2;++channel)mixer->accumulator_[size_t(frame)*2+channel]=
                static_cast<int32_t>(mixer->accumulator_[size_t(frame)*2+channel]*gain);
        }
        if(std::any_of(mixer->accumulator_.begin(),mixer->accumulator_.begin()+count*2,[](int32_t sample){return sample!=0;}))
            mixer->songBlocks_.fetch_add(1,std::memory_order_relaxed);
        bool effect=false;
        for (size_t sound = 0; sound < mixer->uiPlayers_.size(); ++sound) {
            if (!mixer->uiLoaded_[sound] || !mixer->uiPlayers_[sound].playing()) continue;
            AudioPlayer::audioFill(mixer->scratch_.data(), count, &mixer->uiPlayers_[sound]);
            effect=effect || std::any_of(mixer->scratch_.begin(),mixer->scratch_.begin()+count*2,[](int16_t sample){return sample!=0;});
            for (int sample = 0; sample < count * 2; ++sample)
                mixer->accumulator_[static_cast<size_t>(sample)] += mixer->scratch_[static_cast<size_t>(sample)];
        }
        if(effect)mixer->effectBlocks_.fetch_add(1,std::memory_order_relaxed);
        for (int sample = 0; sample < count * 2; ++sample) {
            output[static_cast<size_t>(offset) * 2 + static_cast<size_t>(sample)] =
                static_cast<int16_t>(std::clamp(mixer->accumulator_[static_cast<size_t>(sample)], -32768, 32767));
        }
    }
}

} // namespace rhythm
