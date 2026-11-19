#pragma once
// Generated from PC TagManager, ancestor SortingGroup and SpriteRenderer.
namespace pc_render_order {
enum class Layer {
    GhostHud=0,
    Default=1,
    Highway=2,
    HighwayOverlay=3,
    TrackHUD=4,
    BeatLines=5,
    Lanes=6,
    Sustains=7,
    Frets=8,
    SPAnimation=9,
    KickOpenNotes=10,
    Notes=11,
    TallFrets=12,
    NotesAfterStrikeline=13,
    Flames=14,
    OverlayHUD=15,
    AdditiveGlow=16,
};
// A group is atomic externally; its sprites retain their internal layer/order.
struct Key { int layer,order; int localLayer=0,localOrder=0; };
constexpr bool before(Key a,Key b) noexcept {
    if(a.layer!=b.layer) return a.layer<b.layer;
    if(a.order!=b.order) return a.order<b.order;
    if(a.localLayer!=b.localLayer) return a.localLayer<b.localLayer;
    return a.localOrder<b.localOrder;
}
constexpr Key at(Layer layer,int order=0) noexcept {return {static_cast<int>(layer),order};}
constexpr Key renderer(int id) noexcept {
    switch(id) {
    case 14434: return {4,1,0,0};
    case 14844: return {3,-1,1,-27001};
    case 14855: return {4,3,1,0};
    case 14860: return {3,-1,16,0};
    case 14878: return {3,-1,16,0};
    case 14964: return {3,-200,0,0};
    case 14990: return {3,0,0,0};
    case 15020: return {1,0,0,0};
    case 15108: return {3,-200,0,0};
    case 15375: return {3,-1,1,-100};
    case 15436: return {3,500,0,0};
    case 15608: return {3,-1,16,0};
    case 15635: return {3,0,0,0};
    case 15764: return {3,-1,16,0};
    case 15969: return {3,-1,1,0};
    case 16070: return {9,10,0,0};
    case 16093: return {15,100,0,0};
    case 16131: return {3,-1,16,0};
    case 16219: return {3,-1,16,0};
    case 16336: return {16,100,0,0};
    case 16791: return {3,-1,16,0};
    case 16857: return {4,0,0,0};
    case 16941: return {3,-200,0,0};
    case 16968: return {3,-200,0,0};
    case 17000: return {4,1,0,0};
    case 17009: return {2,0,0,0};
    case 17098: return {3,-1,1,-1};
    case 17107: return {4,3,1,0};
    case 17172: return {4,3,1,3};
    case 17190: return {4,1,0,0};
    case 17404: return {9,10,0,0};
    case 17407: return {3,10,0,0};
    case 17423: return {4,1,0,0};
    case 17482: return {3,-1,16,0};
    case 17617: return {1,0,0,0};
    case 17883: return {3,-1,16,0};
    case 17947: return {4,0,0,0};
    case 17996: return {3,-200,0,0};
    case 17997: return {3,500,0,0};
    case 18049: return {3,-1,16,0};
    case 18156: return {3,-1,1,-1};
    case 18166: return {3,-1,4,160};
    case 18205: return {3,-1,1,-5};
    default: return at(Layer::Default);
    }
}
inline constexpr Key receptors[5][8] = {
    {{8,-100,0,0},{8,0,0,0},{8,1,0,0},{8,1,0,0},{8,2,0,0},{8,3,0,0},{8,4,0,0},{8,5,0,0}},
    {{8,-100,0,0},{8,0,0,0},{8,1,0,0},{8,2,0,0},{8,2,0,0},{8,3,0,0},{8,4,0,0},{8,5,0,0}},
    {{8,-100,0,0},{8,0,0,0},{8,1,0,0},{8,2,0,0},{8,2,0,0},{8,3,0,0},{8,4,0,0},{8,5,0,0}},
    {{8,-100,0,0},{8,0,0,0},{8,1,0,0},{8,2,0,0},{8,2,0,0},{8,3,0,0},{8,4,0,0},{8,5,0,0}},
    {{8,-100,0,0},{8,0,0,0},{8,1,0,0},{8,2,0,0},{8,2,0,0},{8,3,0,0},{8,4,0,0},{8,5,0,0}},
};
}
