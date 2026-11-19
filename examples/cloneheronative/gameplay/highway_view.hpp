#pragma once
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <array>

namespace rhythm {
// SpriteAttach.Update 0x180274380 and scale helper 0x18039B070: orient
// the sprite's local Y along projected world-forward, then match its screen
// height to the serialized top/bottom anchors. Scale offsets use (N-1)/3.
// All points are in the existing native Z-reversed world coordinates.
inline std::array<glm::vec3,4> pcAttachedString(
    const std::array<glm::vec3,4>& saved,const glm::vec3& top,const glm::vec3& bottom,
    const glm::vec2& scaleOffset,const glm::mat4& camera,const glm::vec2& viewport,
    unsigned players,float& scale) noexcept {
    const auto project=[&](const glm::vec3& point) {
        const auto clip=camera*glm::vec4(point,1);
        return glm::vec2(clip)/clip.w*viewport*.5f;
    };
    const auto baseBottom=(saved[2]+saved[3])*.5f;
    const auto baseTop=(saved[0]+saved[1])*.5f;
    const auto axisY=glm::normalize(glm::vec2(baseTop-baseBottom));
    const glm::vec2 axisX(axisY.y,-axisY.x);
    const auto direction=project(bottom+glm::vec3(0,0,-1))-project(bottom);
    const float angle=std::atan2(-direction.y,direction.x)-1.57079632679f;
    const glm::vec2 right(std::cos(angle),std::sin(angle)),up(-right.y,right.x);
    const float count=float(std::clamp(players,1u,4u))-1;
    const glm::vec2 offset=glm::vec2(1)+scaleOffset*(count/3);
    const auto points=[&](float amount) {
        std::array<glm::vec3,4> result;
        for(unsigned i=0;i<4;++i) {
            const auto relative=glm::vec2(saved[i]-baseBottom);
            const auto xy=(right*(glm::dot(relative,axisX)*offset.x)+up*(glm::dot(relative,axisY)*offset.y))*amount;
            result[i]=bottom+glm::vec3(xy,0);
        }
        return result;
    };
    const auto current=points(scale);
    const float currentHeight=glm::length(project((current[0]+current[1])*.5f)-project((current[2]+current[3])*.5f));
    const float desired=glm::length(project(top)-project(bottom));
    if(currentHeight>1e-6f && std::isfinite(desired))scale*=desired/currentHeight;
    return points(scale);
}

struct PcAttachedMeterPose {
    glm::vec3 origin{},right{1,0,0},up{0,1,0};float scale{1};
    glm::vec3 world(const glm::vec3& local) const noexcept {return origin+right*local.x+up*local.y+glm::vec3(0,0,local.z);}
};
template<class Group> inline PcAttachedMeterPose pcMeterAttachmentPose(const Group& group,
    const glm::mat4& camera,const glm::vec2& viewport,float& scale) noexcept {
    const auto v=[](const float* p){return glm::vec3(p[0],p[1],p[2]);};
    const auto project=[&](const glm::vec3& p){const auto c=camera*glm::vec4(p,1);return glm::vec2(c)/c.w*viewport*.5f;};
    PcAttachedMeterPose pose;pose.origin=v(group.origin);
    const auto direction=project(pose.origin+glm::vec3(0,0,-1))-project(pose.origin);
    const float angle=std::atan2(-direction.y,direction.x)-1.57079632679f;
    pose.right=glm::vec3(std::cos(angle),std::sin(angle),0);pose.up=glm::vec3(-pose.right.y,pose.right.x,0);
    const auto point=[&](unsigned i){return pose.world(v(group.referenceFixed[i])+v(group.referenceScaled[i])*scale);};
    const float current=glm::length(project((point(0)+point(1))*.5f)-project((point(2)+point(3))*.5f));
    const float desired=glm::length(project(v(group.top))-project(v(group.bottom)));
    if(current>1e-6f && std::isfinite(desired))scale*=desired/current;
    pose.scale=scale;return pose;
}
template<class Layer> inline glm::vec3 pcAttachedMeterPoint(const Layer& layer,
    const PcAttachedMeterPose& pose,unsigned corner,const glm::vec3& arrowDelta) noexcept {
    const auto v=[](const float* p){return glm::vec3(p[0],p[1],p[2]);};
    auto local=v(layer.fixed[corner])+v(layer.scaled[corner])*pose.scale;
    for(unsigned axis=0;axis<3;++axis)local+=(v(layer.arrowFixed[axis])+v(layer.arrowScaled[axis])*pose.scale)*arrowDelta[axis];
    return pose.world(local);
}

// Native percentage-length adaptation. One matrix is shared by the GPU highway
// and projected source HUD quads. Note speed is deliberately a scrolling input.
inline glm::mat4 highwayMatrix(const glm::mat4& camera,int lengthPercent,bool lefty) noexcept {
    glm::mat4 profile(1.0f);
    profile[0][0]=lefty?-1.0f:1.0f;
    profile[2][2]=static_cast<float>(lengthPercent)/100.0f;
    return camera*profile;
}
inline glm::vec2 highwayScreenPoint(const glm::mat4& matrix,const glm::vec3& point,
                                    const glm::vec2& display) noexcept {
    const glm::vec4 clip=matrix*glm::vec4(point,1.0f);
    if(clip.w<=0.0001f) return glm::vec2(-10000.0f);
    return (glm::vec2(clip)/clip.w*0.5f+0.5f)*display;
}
// Keep each source meter assembly on its reference sprite plane before the
// native Z-length adaptation. Source arrow travel is in Z while its bar sprite
// is in XY; scaling their depths separately otherwise detaches the arrow.
// Ray/plane intersection preserves the original 100% projection exactly.
inline glm::vec3 highwayMeterPoint(const glm::vec3& point,float planeZ,const glm::vec3& eye) noexcept {
    const float depth=point.z-eye.z;
    if(std::abs(depth)<0.0001f) return point;
    return eye+(point-eye)*((planeZ-eye.z)/depth);
}
// The PC streak strip is an XY billboard, while its ten markers are floor
// sprites. SpriteAttach 24081 is disabled in the shipped prefab (and in live
// gameplay), so preserve the authored picture. Put the strip on the markers'
// floor through the reference camera before stretching depth. All eleven
// sprites then receive one projective transform, retaining their containment,
// relative scale and rail direction at every native highway length.
inline glm::vec3 highwayStreakPoint(const glm::vec3& point,const glm::vec3& eye) noexcept {
    const float height=point.y-eye.y;
    if(std::abs(height)<0.0001f) return point;
    return eye+(point-eye)*(-eye.y/height);
}

namespace pc_meter {
// HealthContainer 0x1800F7310; SPBar 0x1801BD6D0. Positions are top/middle/bottom.
inline glm::vec3 position(const float (&positions)[3][3],float amount) noexcept {
    amount=std::clamp(amount,0.0f,1.0f);
    const unsigned segment=amount>=0.5f?0:1;
    const float t=amount>=0.5f?(amount-0.5f)*2.0f:amount*2.0f;
    const auto& a=positions[segment+1];const auto& b=positions[segment];
    return glm::vec3(a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t,a[2]+(b[2]-a[2])*t);
}
// ComboColor 0x1801B9B80: exact tier boundaries retain all ten lit ticks;
// the next hit begins the next ten-note segment. Maximum tier stays full.
constexpr unsigned ticks(unsigned streak) noexcept {
    return streak>=30 || (streak>0 && streak%10==0)?10:streak%10;
}
}
}
