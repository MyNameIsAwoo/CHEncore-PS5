#include "gameplay/usb_source_slots.hpp"
#include <cassert>
#include <cstdio>
int main(){
 std::array<bool,16> active{};std::array<int,16> slots;slots.fill(-1);
 active[5]=true;rhythm::controls::assignUsbSources(active,slots);assert(slots[5]==0);
 active[0]=true;active[10]=true;active[15]=true;rhythm::controls::assignUsbSources(active,slots);
 assert(slots[5]==0 && slots[0]==1 && slots[10]==2 && slots[15]==3);
 active[6]=true;rhythm::controls::assignUsbSources(active,slots);assert(slots[6]==-1);
 active[0]=false;rhythm::controls::assignUsbSources(active,slots);assert(slots[6]==1 && slots[5]==0 && slots[10]==2 && slots[15]==3);
 std::puts("PASS mixed USB/receiver channels preserve slots, ignore empty channels and reuse disconnected slots");
}
