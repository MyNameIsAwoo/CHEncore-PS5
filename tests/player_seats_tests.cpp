#include "../examples/cloneheronative/gameplay/player_seats.hpp"
#include <cassert>
#include <cstdio>
int main(){
 rhythm::controls::PlayerSeats seats;
 assert(seats.join(3)==0 && seats.join(0)==1 && seats.join(6)==2 && seats.join(1)==3);
 assert(seats.join(3)==0 && seats.join(7)==-1);
 seats.erase(1);assert(seats.count==3 && seats.slotOfSeat[1]==6 && seats.slotOfSeat[2]==1 && seats.seatOfSlot[0]==-1);
 assert(seats.join(0)==3);seats.erase(0);assert(seats.slotOfSeat[0]==6 && seats.seatOfSlot[6]==0);
 seats.erase(2);seats.erase(1);seats.erase(0);assert(seats.count==0);
 for(int i:seats.slotOfSeat)assert(i==-1);for(int i:seats.seatOfSlot)assert(i==-1);
 puts("PASS join-order numbering, duplicate join, full seats, dropout/disconnect compaction and reuse");
}
