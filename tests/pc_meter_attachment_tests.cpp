// Copyright (C) 2026 HOWL
// SPDX-License-Identifier: MIT
#include "gameplay/highway_view.hpp"
#include "gameplay/pc_player_camera.hpp"
#include "pc_meter_attachments.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cassert>
#include <cstdio>
int main() {
    unsigned checks=0;
    for(unsigned players=1;players<=4;++players)for(int length:{20,80,100,150,200,1000})for(float tilt:{-2.f,0.f,2.f}) {
        const auto camera=rhythm::pcPlayerCamera(players);
        const glm::vec3 eye(0,camera.height*5,camera.z*5);
        auto proj=glm::perspective(glm::radians(20.f),(1920.f/players)/1080,.1f,200.f);proj[1][1]*=-1;
        auto view=glm::lookAt(eye,glm::vec3(0,tilt,-camera.targetZ*5),glm::vec3(0,1,0));
        const auto matrix=rhythm::highwayMatrix(proj*view,length,false);
        const glm::vec2 viewport(1920.f/players,1080);
        const auto v=[](const float* p){return glm::vec3(p[0],p[1],p[2]);};
        const auto screen=[&](const glm::vec3& p){return rhythm::highwayScreenPoint(matrix,p,viewport);};
        for(const auto& group:pc_meter_attach::groups) {
            float scale=1;rhythm::PcAttachedMeterPose pose;
            for(unsigned frame=0;frame<100;++frame)pose=rhythm::pcMeterAttachmentPose(group,matrix,viewport,scale);
            assert(std::isfinite(scale) && scale>0 && scale<100);
            const auto point=[&](unsigned i){return pose.world(v(group.referenceFixed[i])+v(group.referenceScaled[i])*pose.scale);};
            const float actual=glm::length(screen((point(0)+point(1))*.5f)-screen((point(2)+point(3))*.5f));
            const float expected=glm::length(screen(v(group.top))-screen(v(group.bottom)));
            assert(std::abs(actual-expected)<.1f);++checks;
        }
    }
    // SpriteAttach's zero scale offsets preserve each original child's X/Y
    // proportions. The animated arrow translation must not change that shape.
    unsigned shapes=0;
    for(const auto& layer:pc_meter_attach::layers) {
        const auto v=[](const float* p){return glm::vec3(p[0],p[1],p[2]);};
        const auto ratio=[&](float scale) {
            rhythm::PcAttachedMeterPose pose;pose.scale=scale;
            const auto p0=rhythm::pcAttachedMeterPoint(layer,pose,0,glm::vec3(.1f,.2f,.3f));
            const auto p1=rhythm::pcAttachedMeterPoint(layer,pose,1,glm::vec3(.1f,.2f,.3f));
            const auto p3=rhythm::pcAttachedMeterPoint(layer,pose,3,glm::vec3(.1f,.2f,.3f));
            return glm::length(p1-p0)/glm::length(p3-p0);
        };
        const float reference=ratio(1);
        for(float scale:{.1f,.5f,2.f,4.f}) {
            assert(std::abs(ratio(scale)-reference)<1e-4f);++shapes;
        }
    }
    std::printf("PASS %u source meter camera/length/tilt anchor combinations and %u frame/fill/arrow aspect checks\n",checks,shapes);
}
