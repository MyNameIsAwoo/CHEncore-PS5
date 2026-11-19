// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace rhythm {
// Bounded CPU equivalent of the recovered cone/burst/constant-rate emitters.
// All lifetimes, dimensions, cone angles and physics inputs come from the PC prefab.
class GuitarParticles {
public:
    struct Particle {
        std::array<float,3> origin{},velocity{};
        float age{},life{},gravity{},size{},simulation{};
        unsigned lane{};bool active{},sustain{};
    };
    static constexpr size_t Capacity=512;
    void reset() noexcept {count_=0;seed_=0x917b019u;}
    void update(float dt) noexcept {
        size_t kept=0;
        for(size_t i=0;i<count_;++i) {
            particles_[i].age+=std::max(0.0f,dt)*particles_[i].simulation;
            if(particles_[i].age<particles_[i].life)particles_[kept++]=particles_[i];
        }
        count_=kept;
    }
    template<class Spec> void emit(const Spec& spec,unsigned lane,bool active,bool sustain) noexcept {
        if(count_==Capacity)return;
        const float azimuth=random()*6.28318530718f;
        const float cosine=1.0f-random()*(1.0f-std::cos(spec.angle*0.01745329252f));
        const float sine=std::sqrt(std::max(0.0f,1-cosine*cosine));
        const std::array<float,3> direction={sine*std::cos(azimuth),sine*std::sin(azimuth),cosine};
        const float radius=std::sqrt(random())*spec.radius,angle=random()*6.28318530718f;
        Particle p;p.life=spec.life;p.gravity=spec.gravity;p.size=spec.size;p.simulation=spec.simulation;
        p.lane=lane;p.active=active;p.sustain=sustain;
        for(unsigned axis=0;axis<3;++axis) {
            p.origin[axis]=spec.origin[axis]+radius*(std::cos(angle)*spec.basis[0][axis]+std::sin(angle)*spec.basis[1][axis]);
            for(unsigned local=0;local<3;++local)p.velocity[axis]+=spec.speed*direction[local]*spec.basis[local][axis];
        }
        particles_[count_++]=p;
    }
    template<class Visitor> void visit(Visitor visitor) const {for(size_t i=0;i<count_;++i)visitor(particles_[i]);}
    size_t size() const noexcept {return count_;}
private:
    float random() noexcept {seed_^=seed_<<13;seed_^=seed_>>17;seed_^=seed_<<5;return static_cast<float>(seed_&0xffffff)/16777216.0f;}
    std::array<Particle,Capacity> particles_{};
    size_t count_{};uint32_t seed_{0x917b019u};
};
}
