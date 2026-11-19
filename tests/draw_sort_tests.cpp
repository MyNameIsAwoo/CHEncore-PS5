#include "gameplay/draw_sort.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
static size_t allocations{};
void* operator new(size_t n){++allocations;if(auto* p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,size_t) noexcept{std::free(p);}
struct Draw{int key;size_t sequence;int original;};
int main(){
 unsigned random=0x81ed;std::vector<Draw> actual,reference;actual.reserve(1000);reference.reserve(1000);
 for(int run=0;run<2000;++run){
  actual.clear();for(int i=0;i<run%1000;++i){random^=random<<13;random^=random>>17;random^=random<<5;actual.push_back({int(random%17),0,i});}
  reference=actual;std::stable_sort(reference.begin(),reference.end(),[](auto a,auto b){return a.key<b.key;});
  const auto before=allocations;rhythm::stableDrawSort(actual,[](int a,int b){return a<b;});assert(allocations==before);
  for(size_t i=0;i<actual.size();++i)assert(actual[i].key==reference[i].key && actual[i].original==reference[i].original);
 }
 puts("PASS 2000 draw lists: identical stable order, zero sort allocations");
}
