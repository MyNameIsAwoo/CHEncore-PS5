// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#include "pc_guitar_assets.hpp"
#include "pc_render_order.hpp"
#include "pc_drums_assets.hpp"
#include <cstdio>
#include "gameplay/pc_drums_visuals.hpp"
int main(){
 for(unsigned square=0;square<2;++square)for(unsigned ready=0;ready<2;++ready)
 for(unsigned active=0;active<2;++active)for(unsigned phrase=0;phrase<2;++phrase)
 for(unsigned dynamics=0;dynamics<3;++dynamics)for(unsigned lane=0;lane<8;++lane){
  const auto& s=pc_drums_art::state(lane,dynamics,phrase,active,ready,square);
  std::printf("%d %.9g %.9g",s.group,s.scale[0],s.scale[1]);
  for(const auto& l:s.layers){std::printf(" %d %d",l.role,l.order);for(float v:l.color)std::printf(" %d",static_cast<int>(v*255+0.5f));}
  std::puts("");
 }
 rhythm::PcDrumFretMotion motion;
 for(unsigned i=0;i<181;++i){const double time=i/60.0;if(i==30 || i==60 || i==90)motion.strike(time);std::printf("# motion %.17g %.9g\n",time,motion.sample(time));}
}
