// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
// Exercise the real platform discovery/decoder against a mocked ScePad service.
#include "../ps5/src/platform.c"
#include <assert.h>

static struct user_list users = {{1,-1,-1,-1}};
static bool standards[5], guitars[5], intercepted, alias_handle, valid_data = true;
static int guitar_class = PAD_CLASS_GUITAR, closes[110], class_reads;
static uint8_t frets, solo;
static bool standard_guitars[5], reuse_special, block_open_ext, duplicate_product;
static bool guitar_ready[5] = {true,true,true,true,true};
static unsigned ext_calls;
static bool rotate_rejected_handles;
static int native_vibrations;
static bool extended_only, reuse_standard, block_standard_open, placeholder_special;
static uint32_t native_buttons;
int ps5_klog_capture_stderr(const char *s) { (void)s; return 0; }
int sceKernelUsleep(uint32_t us) { (void)us; return 0; }
int sceUserServiceInitialize(const void *p) { (void)p; return 0; }
int sceUserServiceGetInitialUser(int32_t *u) { *u=1; return 0; }
int sceUserServiceGetLoginUserIdList(struct user_list *list) { *list=users; return 0; }
int scePadInit(void) { return 0; }
int scePadOpen(int32_t user, int32_t port, int32_t index, const void *params) {
   assert(port==0 && index==0 && !params); return block_standard_open?-1:10+user;
}
int scePadGetHandle(int32_t user,int32_t port,int32_t index) {
   assert(index==0);
   if (port==PAD_PORT_STANDARD && reuse_standard) return 10+user;
   return reuse_special && port==PAD_PORT_INSTRUMENT && guitars[user]?100+user:-1;
}
int scePadOpenExt(int32_t user,int32_t port,int32_t index,const void *p) {
   const struct instrument_open_params *params=p;
   assert(port==2 && index==0 && params->vendor==0x0738 && params->product==0x8261);
   assert(params->product_extra==0 || params->product_extra==params->product);
   ++ext_calls;
   if(block_open_ext || (duplicate_product && params->product_extra!=params->product))return -2;
   for (unsigned i=0;i<sizeof(params->reserved);++i)assert(!params->reserved[i]);
   if(rotate_rejected_handles)return 101+(ext_calls%4);
   return alias_handle?10+user:(guitars[user] || placeholder_special)?100+user:-1;
}
int scePadGetControllerInformation(int32_t handle,void *p) {
   assert((handle>100 && handle<105) || (handle>10 && handle<15));
   if(extended_only)return -3;
   struct instrument_info *info=p;memset(info,0,sizeof(*info));
   info->device_class=handle>100?guitar_class:standard_guitars[handle-10]?PAD_CLASS_GUITAR:0;
   info->connected=handle>100?guitars[handle-100]:standards[handle-10];info->connection_type=2;return 0;
}
int scePadGetExtControllerInformation(int32_t handle,void *p) {
   bool saved=extended_only;extended_only=false;
   int result=scePadGetControllerInformation(handle,p);extended_only=saved;return result;
}
int scePadReadState(int32_t handle,void *p) {
   struct pad_sample *s=p;memset(s,0,sizeof(*s));
   s->left_x=s->left_y=s->right_x=s->right_y=128;
   s->connected=handle>100?guitars[handle-100] && guitar_ready[handle-100]:standards[handle-10];
   s->timestamp_us=123456;
   s->buttons=(handle>100 || standard_guitars[handle-10])?native_buttons | (intercepted?PAD_INTERCEPTED:0):PAD_CIRCLE;
   return 0;
}
int scePadDeviceClassParseData(int32_t handle,const void *s,void *p) {
   assert(s && (handle>100 || (handle>10 && handle<15 && standard_guitars[handle-10])));++class_reads;
   struct instrument_data *d=p;memset(d,0,sizeof(*d));
   d->device_class=guitar_class;d->valid=valid_data;d->data.guitar.frets=frets;d->data.guitar.solo_frets=solo;return 0;
}
int scePadClose(int32_t handle) { assert(handle>=0 && handle<110);++closes[handle];return 0; }
int scePadSetVibrationMode(int32_t handle,int32_t mode) {(void)handle;(void)mode;return 0;}
int scePadSetVibration(int32_t handle,const void *data) {(void)handle;(void)data;++native_vibrations;return 0;}
static void poll(void) { struct pad p={0};pad_next_scan=now_seconds()+60;pad_poll(&p); }
static int guitar_slot(int user) {
   for(int i=0;i<PAD_PLAYERS;++i)if(pad_states[i].handle==100+user)return i;
   return -1;
}
int main(void) {
   standards[1]=true;assert(pad_open());poll();assert(pad_players()==1);
   alias_handle=true;pad_scan();assert(!closes[11] && guitar_slot(1)<0);alias_handle=false;
   guitars[1]=true;pad_scan();const int slot=guitar_slot(1);assert(slot>0);
   assert(pad_player_instrument(slot) && !pad_player_instrument(0));
   assert(pad_states[0].handle==11 && !closes[11]);
   native_buttons=PAD_OPTIONS|PAD_UP;frets=1|16;solo=8;poll();
   struct pad p={0};assert(pad_player(slot,&p));
   assert(p.held==(PAD_OPTIONS|PAD_UP|PAD_CROSS|PAD_L1|PAD_TRIANGLE));
   const struct pad_reading *r;assert(pad_player_readings(slot,&r)==1);
   assert(r->buttons==p.held && r->timestamp_us==123456);
   // Both fret banks, every fret, chords, release and rapid alternating strums.
   const uint32_t buttons[]={PAD_CROSS,PAD_CIRCLE,PAD_SQUARE,PAD_TRIANGLE,PAD_L1};
   for(unsigned bank=0;bank<2;++bank)for(unsigned i=0;i<5;++i) {
      frets=bank?0:1u<<i;solo=bank?1u<<i:0;native_buttons=0;poll();
      assert(pad_player(slot,&p) && p.held==buttons[i]);
   }
   frets=solo=0;
   for(unsigned i=0;i<20;++i) {native_buttons=i%2?PAD_DOWN:PAD_UP;poll();pad_player(slot,&p);assert(p.pressed==native_buttons);}
   native_buttons=0;poll();pad_player(slot,&p);assert(p.held==0);
   // Shell input is suppressed without asking the class decoder to reinterpret it.
   int before=class_reads;intercepted=true;frets=31;poll();pad_player(slot,&p);
   assert(!p.held && class_reads==before);intercepted=false;
   // Invalid class data must preserve authentic native controls, not phantom frets.
   valid_data=false;native_buttons=PAD_OPTIONS;poll();pad_player(slot,&p);assert(p.held==PAD_OPTIONS);
   valid_data=true;frets=128;poll();pad_player(slot,&p);assert(p.held==PAD_OPTIONS);
   frets=0;
   // Disconnect clears input, closes only its own handle and allows reconnection.
   guitars[1]=false;poll();assert(!pad_player_instrument(slot));assert(!pad_player(slot,&p) && !p.held);pad_scan();assert(closes[101]==1 && !closes[11]);
   guitars[1]=true;pad_scan();assert(guitar_slot(1)>=0);
   // Sign-out frees both ports for that user, never a different user's controller.
   users.user_id[0]=2;standards[2]=true;pad_scan();assert(closes[101]==2 && closes[11]==1);
   // A wrongly classified special controller is closed, never exposed as guitar.
   guitar_class=0;guitars[2]=true;pad_scan();assert(guitar_slot(2)<0 && closes[102]);guitars[2]=false;
   // Four instruments can replace disconnected standard handles without sharing streams.
   for(int i=0;i<PAD_PLAYERS;++i)pad_player_close(i,"test reset");
   for(int i=1;i<=4;++i){users.user_id[i-1]=i;standards[i]=false;guitars[i]=true;}
   guitar_class=1;pad_scan();poll();assert(pad_players()==15);
   for(int i=1;i<=4;++i)assert(guitar_slot(i)>=0);
   // A connected guitar can be exposed on the standard port. The same class
   // decoder must supply canonical frets and preserve Options/strum there.
   for(int i=0;i<PAD_PLAYERS;++i)pad_player_close(i,"standard guitar case");
   users=(struct user_list){{1,-1,-1,-1}};
   memset(guitars,0,sizeof(guitars));memset(standards,0,sizeof(standards));
   standards[1]=true;standard_guitars[1]=true;pad_scan();poll();
   assert(pad_states[0].port==PAD_PORT_STANDARD && pad_player_instrument(0));
   frets=4;solo=8;native_buttons=PAD_OPTIONS|PAD_DOWN;poll();p=(struct pad){0};
   assert(pad_player(0,&p) && p.held==(PAD_OPTIONS|PAD_DOWN|PAD_SQUARE|PAD_TRIANGLE));
   // The service can replace a standard-port device without changing its handle.
   before=native_vibrations;pad_player_vibrate(0,1,1);assert(native_vibrations==before);
   standard_guitars[1]=false;pad_scan();assert(!pad_player_instrument(0));
   pad_player_vibrate(0,1,1);assert(native_vibrations==before+1);
   standard_guitars[1]=true;pad_scan();assert(pad_player_instrument(0));
   // A known special handle can already be open. Do not require OpenExt to
   // succeed again, and never close a handle borrowed from another owner.
   for(int i=0;i<PAD_PLAYERS;++i)pad_player_close(i,"borrowed guitar case");
   standard_guitars[1]=false;standards[1]=false;guitars[1]=true;
   reuse_special=true;block_open_ext=true;before=closes[101];pad_scan();poll();
   int borrowed=guitar_slot(1);assert(borrowed>=0 && !pad_states[borrowed].owned);
   assert(pad_player_instrument(borrowed));pad_player_close(borrowed,"borrowed release");assert(closes[101]==before);
   reuse_special=false;block_open_ext=false;
   // Some service variants accept the duplicated second product field only.
   duplicate_product=true;unsigned calls=ext_calls;pad_scan();poll();
   assert(guitar_slot(1)>=0 && ext_calls>=calls+2);duplicate_product=false;
   // Successful opens can precede their first connected sample. Keep the
   // class-verified handle long enough for publication, rather than reopening
   // it every scan and restarting the service's initialization.
   for(int i=0;i<PAD_PLAYERS;++i)pad_player_close(i,"delayed connection case");
   guitar_ready[1]=false;before=closes[101];pad_scan();int delayed=guitar_slot(1);
   assert(delayed>=0 && pad_states[delayed].waiting_first_sample && !pad_states[delayed].connected);
   pad_scan();assert(guitar_slot(1)==delayed && closes[101]==before);
   guitar_ready[1]=true;poll();assert(pad_player_instrument(delayed) && !pad_states[delayed].waiting_first_sample);
   guitars[1]=false;poll();pad_scan();assert(closes[101]==before+1);
   // A device that never publishes input is released after the bounded grace.
   guitars[1]=true;guitar_ready[1]=false;pad_scan();delayed=guitar_slot(1);assert(delayed>=0);
   block_open_ext=true;pad_states[delayed].first_sample_deadline=now_seconds()-1;
   before=closes[101];pad_scan();assert(closes[101]==before+1 && guitar_slot(1)<0);
   block_open_ext=false;guitar_ready[1]=true;

   // The extended information query is the only class source on some paths.
   for(int i=0;i<PAD_PLAYERS;++i)pad_player_close(i,"extended information case");
   extended_only=true;guitars[1]=true;pad_scan();poll();assert(guitar_slot(1)>=0 && pad_player_instrument(guitar_slot(1)));
   extended_only=false;
   // A standard handle can also be pre-opened. Preserve it on release.
   for(int i=0;i<PAD_PLAYERS;++i)pad_player_close(i,"borrowed standard case");
   guitars[1]=false;standards[1]=standard_guitars[1]=true;reuse_standard=block_standard_open=true;
   before=closes[11];pad_scan();poll();assert(pad_player_instrument(0) && !pad_states[0].owned);
   pad_player_close(0,"borrowed standard release");assert(closes[11]==before);
   reuse_standard=block_standard_open=false;standard_guitars[1]=false;
   // Never let disconnected special placeholders starve real standard pads.
   for(int i=0;i<PAD_PLAYERS;++i)pad_player_close(i,"placeholder isolation case");
   placeholder_special=true;
   for(int i=1;i<=4;++i){users.user_id[i-1]=i;guitars[i]=false;standards[i]=true;}
   pad_scan();poll();assert(pad_players()==15);
   for(int i=0;i<PAD_PLAYERS;++i)assert(pad_states[i].port==PAD_PORT_STANDARD && !pad_player_instrument(i));
   placeholder_special=false;

   // A service may allocate a fresh, unusable handle on every scan. That is
   // still the same rejection status, not a reason to spam the tester logger.
   for(int i=0;i<PAD_PLAYERS;++i)pad_player_close(i,"rejection logging case");
   users=(struct user_list){{1,-1,-1,-1}};
   guitar_class=0;rotate_rejected_handles=true;pad_scan();
   char discard[256];while(pad_diagnostic(discard,sizeof(discard))){}
   for(int scan=0;scan<5;++scan)pad_scan();
   assert(!pad_diagnostic(discard,sizeof(discard)));
   rotate_rejected_handles=false;guitar_class=PAD_CLASS_GUITAR;
   pad_event("test diagnostic queue");

   // Events are bounded and drained independently of filesystem/network I/O.
   char event[256];unsigned count=0;while(pad_diagnostic(event,sizeof(event)))++count;
   assert(count>0 && count<=16);
   puts("PASS native RB4 standard/special ports, borrowed handles, delayed publication, bounded timeout, product variants, fret banks, strum edges, interception and four instruments");
}
