// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
// Included by audio_player.cpp: shares its validated container probes/helpers.
namespace {
struct StreamDecoder {
 AudioInfo info{};std::string path;
 FILE* opusFile{};OpusDecoder* opus{};stb_vorbis* vorbis{};drwav* wav{};drmp3* mp3{};
 std::array<uint8_t,255> laces{};std::array<uint8_t,65025> payload{};
 std::vector<uint8_t> packet;size_t segment{},segments{},payloadAt{};
 unsigned headers{},preSkip{};uint32_t serial{};bool haveSerial{},bad{};
 uint64_t decoded{},skipUntil{},outAt{},sourceBase{};size_t rawAt{},rawCount{};
 std::array<int16_t,5760*2> raw{};std::array<int16_t,2> previous{},current{};bool interpolator{};
 ~StreamDecoder(){close();}
 void close() noexcept {
  if(opus)opus_decoder_destroy(opus);opus=nullptr;
  if(opusFile)std::fclose(opusFile);opusFile=nullptr;
  if(vorbis)stb_vorbis_close(vorbis);vorbis=nullptr;
  if(wav){drwav_uninit(wav);delete wav;}wav=nullptr;
  if(mp3){drmp3_uninit(mp3);delete mp3;}mp3=nullptr;
 }
 bool open(const std::string& name) noexcept {
  path=name;if(!probeAudio(path.c_str(),info))return false;
  if(info.opus){packet.reserve(MaxPacketBytes);opusFile=std::fopen(path.c_str(),"rb");return opusFile!=nullptr;}
  FILE* file=std::fopen(path.c_str(),"rb");if(!file)return false;uint8_t magic[12]{};
  const bool ok=readExact(file,magic,12);std::fclose(file);if(!ok)return false;
  if(!std::memcmp(magic,"OggS",4)){int error=0;vorbis=stb_vorbis_open_filename(path.c_str(),&error,nullptr);if(!vorbis)return false;const auto actual=stb_vorbis_get_info(vorbis);return actual.channels==info.channels && actual.sample_rate==info.rate;}
  if(!std::memcmp(magic,"RIFF",4)){wav=new(std::nothrow) drwav{};const bool initialized=wav && drwav_init_file(wav,path.c_str(),nullptr);if(initialized && wav->channels==unsigned(info.channels) && wav->sampleRate==info.rate)return true;if(initialized)drwav_uninit(wav);delete wav;wav=nullptr;return false;}
  mp3=new(std::nothrow) drmp3{};const bool initialized=mp3 && drmp3_init_file(mp3,path.c_str(),nullptr);if(initialized && mp3->channels==unsigned(info.channels) && mp3->sampleRate==info.rate)return true;if(initialized)drmp3_uninit(mp3);delete mp3;mp3=nullptr;return false;
 }
 bool nextPacket(const std::atomic<bool>* stop) noexcept {
  packet.clear();
  for(;;){
   if(stop && stop->load(std::memory_order_relaxed))return false;
   if(segment==segments){
    uint8_t h[27];if(!readExact(opusFile,h,27) || std::memcmp(h,"OggS",4) || h[4])return false;
    const auto id=readU32(h+14);if(!haveSerial){serial=id;haveSerial=true;if(!(h[5]&2))return false;}
    if(id!=serial || bool(h[5]&1)!=!packet.empty())return false;
    segments=h[26];segment=payloadAt=0;
    if(!readExact(opusFile,laces.data(),segments))return false;
    size_t size=0;for(size_t i=0;i<segments;++i)size+=laces[i];
    if(!readExact(opusFile,payload.data(),size))return false;
   }
   while(segment<segments){
    const size_t bytes=laces[segment++];if(packet.size()+bytes>MaxPacketBytes)return false;
    packet.insert(packet.end(),payload.data()+payloadAt,payload.data()+payloadAt+bytes);payloadAt+=bytes;
    if(bytes<255)return true;
   }
  }
 }
 bool opusBlock(const std::atomic<bool>* stop) noexcept {
  while(!stop || !stop->load(std::memory_order_relaxed)){
   if(!nextPacket(stop))return false;
   if(!headers){
    if(packet.size()<19 || std::memcmp(packet.data(),"OpusHead",8) || packet[9]!=info.channels || packet[18])return false;
    preSkip=packet[10]|(unsigned(packet[11])<<8);int error=0;
    opus=opus_decoder_create(48000,info.channels,&error);if(!opus || error)return false;++headers;continue;
   }
   if(headers==1){if(packet.size()<8 || std::memcmp(packet.data(),"OpusTags",8))return false;++headers;continue;}
   const int count=opus_packet_get_nb_samples(packet.data(),int(packet.size()),48000);
   if(count<=0 || count>5760)return false;
   const uint64_t start=decoded;decoded+=uint64_t(count);
   // One second of decoder history before a seek; no prefix PCM is retained.
   if(decoded+48000<=skipUntil)continue;
   if(opus_decode(opus,packet.data(),int(packet.size()),raw.data(),5760,0)!=count)return false;
   const auto from=std::max(start,std::max(uint64_t(preSkip),skipUntil));
   const auto to=std::min(decoded,uint64_t(preSkip)+info.frames48);
   if(to<=from)continue;
   rawAt=size_t(from-start);rawCount=size_t(to-start);return true;
  }
  return false;
 }
 bool seek(uint64_t frame) noexcept {
  outAt=std::min(frame,info.frames48);rawAt=rawCount=0;interpolator=false;bad=false;
  const uint64_t source=outAt*info.rate/48000;sourceBase=source;
  if(outAt==info.frames48)return true;
  if(info.opus){
   if(opus)opus_decoder_destroy(opus);opus=nullptr;
   if(std::fseek(opusFile,0,SEEK_SET))return false;
   headers=0;segment=segments=payloadAt=0;haveSerial=false;decoded=0;
   // preSkip from the original probe header will be parsed before decoding.
   uint8_t h[27],l[255],id[19];
   if(!readExact(opusFile,h,27) || !readExact(opusFile,l,h[26]) || !readExact(opusFile,id,19))return false;
   preSkip=id[10]|(unsigned(id[11])<<8);skipUntil=outAt+preSkip;
   return std::fseek(opusFile,0,SEEK_SET)==0;
  }
  if(vorbis)return stb_vorbis_seek(vorbis,unsigned(source))!=0;
  if(wav)return drwav_seek_to_pcm_frame(wav,source)!=0;
  return mp3 && drmp3_seek_to_pcm_frame(mp3,source)!=0;
 }
 bool sample(std::array<int16_t,2>& result,const std::atomic<bool>* stop) noexcept {
  if(rawAt==rawCount){
   rawAt=rawCount=0;
   if(info.opus){if(!opusBlock(stop))return false;}
   else if(vorbis)rawCount=size_t(stb_vorbis_get_samples_short_interleaved(vorbis,info.channels,raw.data(),int(raw.size())));
   else if(wav)rawCount=size_t(drwav_read_pcm_frames_s16(wav,raw.size()/2,raw.data()));
   else if(mp3)rawCount=size_t(drmp3_read_pcm_frames_s16(mp3,raw.size()/2,raw.data()));
   if(mp3 && (mp3->channels!=unsigned(info.channels) || mp3->sampleRate!=info.rate))return false;
   if(!rawCount)return false;
  }
  result={raw[rawAt*info.channels],raw[rawAt*info.channels+(info.channels==2?1:0)]};++rawAt;return true;
 }
 size_t read(int16_t* output,size_t frames,const std::atomic<bool>* stop) noexcept {
  frames=size_t(std::min<uint64_t>(frames,info.frames48-outAt));size_t done=0;
  for(;done<frames;++done){
   if(stop && stop->load(std::memory_order_relaxed))break;
   std::array<int16_t,2> value{};
   if(info.rate==48000){if(!sample(value,stop)){bad=true;break;}}
   else {
    if(!interpolator){if(!sample(previous,stop)){bad=true;break;}if(!sample(current,stop)){if(sourceBase+1<info.nativeFrames){bad=true;break;}current=previous;}interpolator=true;}
    const auto phase=outAt*info.rate,base=phase/48000,fraction=phase%48000;
    while(sourceBase<base){previous=current;if(!sample(current,stop)){if(sourceBase+2<info.nativeFrames){bad=true;break;}current=previous;}++sourceBase;}
    if(bad)break;
    for(size_t c=0;c<2;++c)value[c]=int16_t((int64_t(previous[c])*int64_t(48000-fraction)+int64_t(current[c])*int64_t(fraction))/48000);
   }
   output[done*2]=value[0];output[done*2+1]=value[1];++outAt;
  }
  return done;
 }
};
}

struct StreamedSong::State {
 static constexpr uint64_t Capacity=131072,Block=4096,Prefill=24000;
 std::array<StreamDecoder*,Stems> decoders{};
 std::array<int16_t*,Stems> rings{};
 std::array<std::atomic<float>,Stems> gains{};
 std::array<int16_t,Block*2> scratch{};
 std::atomic<bool> stop{},error{},playing{};
 std::atomic<uint64_t> requested{1},active{},target{},cursor{},written{},end{},step{AudioPhaseOne},starved{};
 std::atomic<unsigned> readers{};
 uint64_t total{},bytes{};pthread_t worker{};bool thread{};
 ~State(){stop=true;if(thread)pthread_join(worker,nullptr);for(auto* d:decoders)delete d;for(auto* p:rings)std::free(p);}
 static void* entry(void* p) noexcept {static_cast<State*>(p)->run();return nullptr;}
 void run() noexcept {
  uint64_t epoch=0,write=0;
  while(!stop.load(std::memory_order_acquire)){
   const auto request=requested.load(std::memory_order_acquire);
   if(epoch!=request){
    active.store(0,std::memory_order_release);
    if(readers.load(std::memory_order_acquire)){usleep(1000);continue;}
    write=target.load(std::memory_order_acquire);
    bool ok=true;for(auto* d:decoders)if(d && !d->seek(std::min(write,d->info.frames48)))ok=false;
    if(!ok){error=true;return;}
    cursor.store(write*AudioPhaseOne,std::memory_order_release);written.store(write,std::memory_order_release);epoch=request;
   }
   const auto read=cursor.load(std::memory_order_acquire)>>32;
   if(write>=total || write-read>=Capacity-Block){
    if(active.load(std::memory_order_relaxed)!=epoch)active.store(epoch,std::memory_order_release);
    usleep(1000);continue;
   }
   const auto count=std::min(Block,total-write);
   for(size_t stem=0;stem<Stems;++stem)if(decoders[stem]){
    auto* d=decoders[stem];scratch.fill(0);
    const auto need=write<d->info.frames48?std::min(count,d->info.frames48-write):0;
    if(need && d->read(scratch.data(),size_t(need),&stop)!=need){if(!stop)error=true;return;}
    for(size_t i=0;i<count;++i){const auto at=((write+i)&(Capacity-1))*2;rings[stem][at]=scratch[i*2];rings[stem][at+1]=scratch[i*2+1];}
   }
   write+=count;written.store(write,std::memory_order_release);
   if(write-std::min(read,write)>=Prefill || write==total)active.store(epoch,std::memory_order_release);
  }
 }
};
StreamedSong* StreamedSong::create(const std::array<std::string,Stems>& paths,const std::atomic<bool>* cancel) noexcept {
 auto* state=new(std::nothrow) State;if(!state)return nullptr;
 bool any=false,ok=true;
 for(size_t i=0;i<Stems;++i){state->gains[i]=1;
  if(paths[i].empty())continue;
  if(cancel && cancel->load(std::memory_order_acquire)){ok=false;break;}
  auto* d=new(std::nothrow) StreamDecoder;
  if(!d || !d->open(paths[i])){delete d;ok=false;break;}
  state->decoders[i]=d;state->rings[i]=static_cast<int16_t*>(std::malloc(State::Capacity*4));
  if(!state->rings[i]){ok=false;break;}
  state->bytes+=State::Capacity*4;state->total=std::max(state->total,d->info.frames48);any=true;
 }
 state->end=state->total;
 if(!ok || !any || pthread_create(&state->worker,nullptr,State::entry,state)!=0){delete state;return nullptr;}
 state->thread=true;
 while(!state->active.load(std::memory_order_acquire) && !state->error.load(std::memory_order_acquire)){
  if(cancel && cancel->load(std::memory_order_acquire)){delete state;return nullptr;}usleep(1000);
 }
 if(state->error){delete state;return nullptr;}
 auto* song=new(std::nothrow) StreamedSong(state);if(!song)delete state;return song;
}
StreamedSong::~StreamedSong(){delete state_;}
void StreamedSong::seek(double seconds) noexcept {
 pause();state_->end=state_->total;if(!std::isfinite(seconds))seconds=0;
 const auto frame=uint64_t(std::clamp(seconds*48000.,0.,double(state_->total)));
 // A no-op seek does not discard a prefetched start buffer.
 if(state_->active.load(std::memory_order_acquire)==state_->requested.load(std::memory_order_acquire) && state_->cursor.load(std::memory_order_acquire)==frame*AudioPhaseOne)return;
 state_->target.store(frame,std::memory_order_release);state_->requested.fetch_add(1,std::memory_order_acq_rel);
}
void StreamedSong::window(double start,double length) noexcept {
 seek(start);if(!std::isfinite(length) || length<0)length=0;
 const auto first=uint64_t(std::clamp(position()*48000.,0.,double(state_->total)));
 state_->end=first+uint64_t(std::min(length*48000.,double(state_->total-first)));
}
bool StreamedSong::play() noexcept {if(failed() || position()*48000.>=double(state_->end.load(std::memory_order_acquire)))return false;state_->playing=true;return true;}
void StreamedSong::pause() noexcept {state_->playing=false;}
bool StreamedSong::playing() const noexcept {return state_->playing.load(std::memory_order_acquire) && !failed();}
void StreamedSong::sourceUnavailable() noexcept {state_->playing=false;state_->error=true;state_->stop=true;}
bool StreamedSong::failed() const noexcept {return state_->error.load(std::memory_order_acquire);}
bool StreamedSong::buffering() const noexcept {
 if(!playing())return false;
 const auto& s=*state_;
 return s.active.load(std::memory_order_acquire)!=s.requested.load(std::memory_order_acquire) ||
        s.written.load(std::memory_order_acquire)<std::min((s.cursor.load(std::memory_order_acquire)>>32)+1024,s.end.load(std::memory_order_acquire));
}
double StreamedSong::position() const noexcept {
 if(state_->active.load(std::memory_order_acquire)!=state_->requested.load(std::memory_order_acquire))return double(state_->target.load(std::memory_order_acquire))/48000.;
 return double(state_->cursor.load(std::memory_order_acquire))/(double(AudioPhaseOne)*48000.);
}
double StreamedSong::duration() const noexcept {return double(state_->total)/48000.;}
uint64_t StreamedSong::pcmBytes() const noexcept {return state_->bytes;}
uint64_t StreamedSong::underruns() const noexcept {return state_->starved.load(std::memory_order_relaxed);}
void StreamedSong::rate(float value) noexcept {if(!std::isfinite(value))value=1;state_->step=uint64_t(std::clamp(value,.25f,2.f)*double(AudioPhaseOne));}
void StreamedSong::gain(size_t stem,float value) noexcept {if(stem<Stems)state_->gains[stem]=std::isfinite(value)?std::clamp(value,0.f,2.f):0.f;}
void StreamedSong::mix(int32_t* output,int frames) noexcept {
 if(!playing() || frames<=0)return;
 auto& s=*state_;s.readers.fetch_add(1,std::memory_order_acq_rel);
 const auto release=[&](){s.readers.fetch_sub(1,std::memory_order_release);};
 const auto epoch=s.requested.load(std::memory_order_acquire);
 if(s.active.load(std::memory_order_acquire)!=epoch){release();return;}
 auto phase=s.cursor.load(std::memory_order_acquire);const auto step=s.step.load(std::memory_order_acquire),end=s.end.load(std::memory_order_acquire)*AudioPhaseOne;
 const auto samples=std::min<uint64_t>(uint64_t(frames),(end-phase+step-1)/step);
 if(samples){const auto last=(phase+(samples-1)*step)>>32;const auto need=std::min(last+2,s.total);
  if(need>s.written.load(std::memory_order_acquire)){s.starved.fetch_add(1,std::memory_order_relaxed);release();return;}}
 for(size_t stem=0;stem<Stems;++stem)if(s.rings[stem]){
  auto at=phase;const auto gain=s.gains[stem].load(std::memory_order_acquire);
  const auto ownEnd=s.decoders[stem]->info.frames48;
  for(size_t i=0;i<samples;++i){const auto first=at>>32;
   if(first>=ownEnd){at+=step;continue;}
   const auto next=std::min(first+1,ownEnd-1);const float fraction=float(at&0xffffffffu)/4294967296.f;
   for(size_t c=0;c<2;++c){const float a=s.rings[stem][(first&(State::Capacity-1))*2+c],b=s.rings[stem][(next&(State::Capacity-1))*2+c];output[i*2+c]+=int16_t(std::clamp((a+(b-a)*fraction)*gain,-32768.f,32767.f));}at+=step;
  }
 }
 phase=std::min(end,phase+samples*step);s.cursor.store(phase,std::memory_order_release);if(phase==end)s.playing=false;release();
}
