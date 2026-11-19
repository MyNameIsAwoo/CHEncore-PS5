#include "gameplay/sustain_mesh.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
void check(bool value) { if (!value) { std::fputs("Sustain mesh check failed\n",stderr);std::exit(1); } }
bool close(float a,float b) { return std::abs(a-b)<.001f; }
std::vector<rhythm::TailSection> mesh(float root,float length,float far=-40,float near=1) {
    std::vector<rhythm::TailSection> out;
    rhythm::submitSustainSections(root,length,5,100.2f,.38f,.70f,far,near,
        [&](auto section) { out.push_back(section); });
    return out;
}
int main() {
    auto full=mesh(0,10);
    check(full.size()==2);
    // PC shader evaluated on a 100.2-unit sliced sprite: top maps to L+.2,
    // its 38-pixel border maps to L-.18. Five native units per PC unit.
    check(close(full[0].nearZ,0) && close(full[0].farZ,-9.1f));
    check(close(full[1].nearZ,-9.1f) && close(full[1].farZ,-11));
    check(close(full[0].nearV,1) && close(full[0].farV,38.f/70));
    check(close(full[0].farV,full[1].nearV) && close(full[1].farV,0));
    auto longer=mesh(0,20);
    check(longer.size()==2);
    check(close(longer[1].nearZ-longer[1].farZ,full[1].nearZ-full[1].farZ));
    auto shortTail=mesh(0,.1f);
    check(shortTail.size()==1 && close(shortTail[0].nearZ,0));
    check(close(shortTail[0].farZ,-1.1f));
    // Highway clipping must crop the UVs instead of stretching the visible cap.
    auto clipped=mesh(0,10,-10);
    check(clipped.size()==2 && close(clipped[1].farZ,-10));
    check(clipped[1].farV>0 && clipped[1].farV<clipped[1].nearV);
    auto nearClipped=mesh(4,10,-40,1);
    check(nearClipped.size()==2 && close(nearClipped[0].nearZ,1));
    check(nearClipped[0].nearV<1 && nearClipped[0].nearV>nearClipped[0].farV);
    check(mesh(0,0).empty() && mesh(0,-1).empty());
    check(mesh(0,std::numeric_limits<float>::quiet_NaN()).empty());
    // Tail shortening keeps the tip at a fixed chart time as the held root anchors.
    auto held=mesh(0,8);
    check(close(held.back().farZ,-9));
    for (const auto& s : nearClipped) {
        check(s.nearZ>s.farZ && s.nearV>=s.farV && s.nearV<=1 && s.farV>=0);
    }
    std::puts("PC sustain slicing, clipping and shortening checks passed");
}
