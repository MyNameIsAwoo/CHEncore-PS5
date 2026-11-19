#include "gameplay/highway_view.hpp"
#include "pc_hud_assets.hpp"
#include "pc_meter_art.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
void check(bool value,const char* label) {if(!value){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}}
bool near(float a,float b) {return std::abs(a-b)<0.002f;}
int main() {
    using namespace rhythm;
    auto projection=glm::perspective(glm::radians(20.0f),16.0f/9.0f,0.1f,500.0f);projection[1][1]*=-1;
    const glm::vec3 eye(0,5,19.85f);
    const glm::mat4 camera=projection*glm::lookAt(eye,eye+glm::vec3(0,-std::sin(glm::radians(6.0f)),-std::cos(glm::radians(6.0f))),glm::vec3(0,1,0));
    const glm::vec2 display(1920,1080);
    for(int length:{20,50,100,200,1000}) for(bool lefty:{false,true}) {
        const auto matrix=highwayMatrix(camera,length,lefty);
        // Every original fail/SP/multiplier layer must project through exactly
        // the same parent transform as the GPU highway, including handedness.
        for(const auto& layer:pc_hud::layers) for(const auto& v:layer.vertices) {
            const glm::vec4 world(lefty?-v[0]:v[0],v[1],v[2]*length/100.0f,1);
            const glm::vec4 clip=camera*world;
            const glm::vec2 expected=(glm::vec2(clip)/clip.w*0.5f+0.5f)*display;
            const auto actual=highwayScreenPoint(matrix,glm::vec3(v[0],v[1],v[2]),display);
            check(std::isfinite(actual.x)&&std::isfinite(actual.y),"finite HUD at supported length extremes");
            check(near(actual.x,expected.x)&&near(actual.y,expected.y),"source HUD shares GPU parent transform");
        }
    }
    // Flattening must leave the PC prefab's baseline picture untouched, and
    // all components of an assembly must have the same parent-plane depth.
    for(const auto& layer:pc_hud::layers) for(const auto& v:layer.vertices) {
        const glm::vec3 original(v[0],v[1],v[2]);
        const float plane=layer.renderer==17947?pc_hud::railMeterPlaneZ:pc_hud::multiplierPlaneZ;
        const auto point=highwayMeterPoint(original,plane,eye);
        check(near(point.z,plane),"meter components share reference depth");
        const auto a=highwayScreenPoint(camera,original,display);
        const auto b=highwayScreenPoint(camera,point,display);
        check(near(a.x,b.x)&&near(a.y,b.y),"PC 100% appearance preserved");
    }
    for(bool lefty:{false,true}) {
        const glm::vec3 shakenEye(.035f,5.01f,19.85f);
        const glm::mat4 shakenCamera=projection*glm::lookAt(shakenEye,shakenEye+glm::vec3(0,-std::sin(glm::radians(6.0f)),-std::cos(glm::radians(6.0f))),glm::vec3(0,1,0));
        const auto matrix=highwayMatrix(shakenCamera,100,lefty);
        const glm::vec3 localEye(lefty?-shakenEye.x:shakenEye.x,shakenEye.y,shakenEye.z);
        const glm::vec3 original(-2.8f,.1f,-6.05f);
        const auto a=highwayScreenPoint(matrix,original,display);
        const auto b=highwayScreenPoint(matrix,highwayMeterPoint(original,pc_hud::railMeterPlaneZ,localEye),display);
        check(near(a.x,b.x)&&near(a.y,b.y),"mirrored shake preserves reference projection");
    }
    // Streak ticks belong to the source highway floor, independently of the
    // circular badge. Flattening them to the badge's Z plane makes them leave
    // the rail direction when highway length changes.
    for(int length:{20,50,100,200,1000}) for(bool lefty:{false,true}) {
        const auto matrix=highwayMatrix(camera,length,lefty);
        for(size_t index=20;index<std::size(pc_meter_art::layers);++index) {
            const auto& layer=pc_meter_art::layers[index];
            glm::vec3 center(0);
            for(const auto& v:layer.vertices) {
                const glm::vec3 original(v[0],v[1],v[2]);
                const auto floor=highwayStreakPoint(original,eye);
                check(near(floor.y,0),"streak tick remains on highway floor");
                const auto actual=highwayScreenPoint(matrix,floor,display);
                const auto expected=highwayScreenPoint(matrix,original,display);
                check(near(actual.x,expected.x)&&near(actual.y,expected.y),"tick keeps source depth at every length");
                center+=floor*.25f;
            }
            const auto tick=highwayScreenPoint(matrix,center,display);
            const auto rail=highwayScreenPoint(matrix,glm::vec3(2.5f,0,center.z),display);
            check(near((tick.x-960)/(rail.x-960),center.x/2.5f),"tick row follows highway rail vanishing point");
        }
        // The shipped PC backing has SpriteAttach disabled. Preserve every
        // source corner at 100%, then keep markers inside that same floor quad
        // at all supported lengths. This catches independent billboard sizing.
        std::array<glm::vec2,4> backing;
        for(unsigned i=0;i<4;++i) {
            const auto& v=pc_meter_art::layers[13].vertices[i];
            const glm::vec3 original(v[0],v[1],v[2]);
            const auto floor=highwayStreakPoint(original,eye);
            check(near(floor.y,0),"strip and ticks share floor depth adaptation");
            backing[i]=highwayScreenPoint(matrix,floor,display);
            if(length==100) {
                const auto expected=highwayScreenPoint(matrix,original,display);
                check(glm::length(backing[i]-expected)<.002f,"live PC authored backing preserved");
            }
        }
        for(size_t index=20;index<std::size(pc_meter_art::layers);++index) {
            for(const auto& v:pc_meter_art::layers[index].vertices) {
                const auto p=highwayScreenPoint(matrix,glm::vec3(v[0],v[1],v[2]),display);
                float sign=0;
                for(unsigned edge=0;edge<4;++edge) {
                    const auto a=backing[edge],d=backing[(edge+1)%4]-a,q=p-a;
                    const float cross=d.x*q.y-d.y*q.x;
                    if(edge==0) sign=cross;
                    check(cross*sign>=0,"every marker corner remains within source backing");
                }
            }
        }

    }
    // A filled bar and its animated arrow must retain their relative position
    // within that assembly at different lengths. This fails with raw Z scaling.
    for(int length:{20,50,100,200,1000}) {
        const auto matrix=highwayMatrix(camera,length,false);
        const float plane=pc_hud::railMeterPlaneZ;
        const glm::vec3 arrow=highwayMeterPoint(glm::vec3(-2.8f,0,-6.05f),plane,eye);
        const glm::vec3 p0(arrow.x,0,plane),p1(arrow.x,1.6f,plane),middle(arrow.x,0.6f,plane);
        const auto crossRatio=[&](const glm::mat4& m) {
            const float a=highwayScreenPoint(m,p0,display).y,b=highwayScreenPoint(m,p1,display).y;
            const float c=highwayScreenPoint(m,middle,display).y,d=highwayScreenPoint(m,arrow,display).y;
            return ((d-a)/(b-a))/((d-c)/(b-c));
        };
        // Cross ratio is invariant under projection of a common plane. With
        // the old independent depth stretch, the arrow leaves this bar line.
        check(near(crossRatio(matrix),crossRatio(camera)),"arrow/bar projective relationship preserved");
    }
    for(const auto* positions:{&pc_hud::healthPositions,&pc_hud::spPositions}) {
        for(int i=0;i<3;++i) {
            const auto p=pc_meter::position(*positions,1.0f-i*0.5f);
            for(int k=0;k<3;++k) check(near(p[k],(*positions)[i][k]),"source meter bottom/middle/top endpoints");
        }
        const auto quarter=pc_meter::position(*positions,0.25f);
        const auto threeQuarter=pc_meter::position(*positions,0.75f);
        check(near(quarter.z,((*positions)[1][2]+(*positions)[2][2])*0.5f),"lower-half meter interpolation");
        check(near(threeQuarter.z,((*positions)[0][2]+(*positions)[1][2])*0.5f),"upper-half meter interpolation");
    }
    check(pc_meter::ticks(0)==0 && pc_meter::ticks(9)==9,"initial combo segment");
    check(pc_meter::ticks(10)==10 && pc_meter::ticks(11)==1,"PC tenth-hit boundary");
    check(pc_meter::ticks(20)==10 && pc_meter::ticks(21)==1,"PC twentieth-hit boundary");
    check(pc_meter::ticks(30)==10 && pc_meter::ticks(99)==10,"maximum tier stays full");
    std::puts("PASS: shared highway/HUD projection at 20-1000%, source meter travel and combo boundaries");
}
