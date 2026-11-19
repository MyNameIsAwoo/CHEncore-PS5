#pragma once
#include <algorithm>
#include <vector>
namespace rhythm {
// Stable PC layer ordering without stable_sort's per-player/frame allocation.
// Explicit submission order makes equal keys unique before in-place sorting.
template<class Draw,class Before>
void stableDrawSort(std::vector<Draw>& draws,Before before) {
 for(size_t i=0;i<draws.size();++i)draws[i].sequence=i;
 std::sort(draws.begin(),draws.end(),[&](const Draw& a,const Draw& b){
  if(before(a.key,b.key))return true;
  if(before(b.key,a.key))return false;
  return a.sequence<b.sequence;
 });
}
}
