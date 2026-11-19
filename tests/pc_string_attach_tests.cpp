// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/highway_view.hpp"
#include "gameplay/pc_player_camera.hpp"
#include "pc_highway_assets.hpp"
#include "pc_string_attach.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cassert>
#include <cstdio>
int main() {
    unsigned comparisons=0;
    for(unsigned players=1;players<=4;++players)for(int length:{20,80,100,150,200}) {
        const auto pose=rhythm::pcPlayerCamera(players);
        const auto eye=glm::vec3(0,pose.height*5,pose.z*5);
        auto proj=glm::perspective(glm::radians(20.f),(1920.f/players)/1080,.1f,200.f);proj[1][1]*=-1;
        const auto view=glm::lookAt(eye,glm::vec3(0,0,-pose.targetZ*5),glm::vec3(0,1,0));
        const auto matrix=rhythm::highwayMatrix(proj*view,length,false);
        const glm::vec2 viewport(1920.f/players,1080);
        for(const auto& source:pc_highway::strings) {
            std::array<glm::vec3,4> saved,attached;
            bool found=false;
            for(const auto& layer:pc_highway::layers)if(layer.renderer==source.renderer) {
                for(unsigned i=0;i<4;++i)saved[i]=glm::vec3(layer.vertices[i][0],layer.vertices[i][1],layer.vertices[i][2]);
                found=true;break;
            }
            assert(found);
            const auto native=[](const float* v){return glm::vec3(v[0]*5,v[1]*5,-v[2]*5);};
            const auto top=native(source.top),bottom=native(source.bottom);
            float scale=1;
            for(unsigned step=0;step<60;++step)attached=rhythm::pcAttachedString(saved,top,bottom,
                glm::vec2(source.scaleOffset[0],source.scaleOffset[1]),matrix,viewport,players,scale);
            assert(std::isfinite(scale) && scale>0 && scale<100);
            const auto point=[&](const glm::vec3& v){return rhythm::highwayScreenPoint(matrix,v,viewport);};
            assert(glm::length((attached[2]+attached[3])*.5f-bottom)<1e-4f);
            const auto actual=glm::length(point((attached[0]+attached[1])*.5f)-point(bottom));
            const auto expected=glm::length(point(top)-point(bottom));
            assert(std::abs(actual-expected)<.1f);++comparisons;
        }
    }
    std::printf("PASS %u PC string anchor/camera/length combinations\n",comparisons);
}
