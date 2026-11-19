#include "pc_render_order.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
void check(bool value,const char* message) {if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main() {
    using namespace pc_render_order;
    // Independent source fixtures: TagManager list and actual renderer IDs.
    check(before(renderer(17997),at(Layer::BeatLines)),"sidebar glow order 500 stays below beat lines");
    check(before(renderer(18166),at(Layer::Sustains)),"track HUD overlay order 160 stays below tails");
    check(before(renderer(17404),at(Layer::KickOpenNotes)),"SP neck animation stays below Open notes");
    check(before(at(Layer::KickOpenNotes),at(Layer::Notes)),"Open notes stay below fretted notes");
    check(before(at(Layer::Notes),renderer(16093)),"overlay HUD arrow stays above notes");
    check(before(receptors[0][7],at(Layer::Notes)),"receptor cover stays below incoming notes");
    check(before(renderer(15108),renderer(15635)),"strings sort below rails within HighwayOverlay");
    // Independent prefab fixtures: Combo SortingGroup 14233 is
    // HighwayOverlay/-1, and health-arrow SortingGroup 14235 is Track HUD/3.
    for(int id:{14844,15375,18205,17098,18156,15969,18166,14860,18049}) {
        const auto key=renderer(id);
        check(key.layer==static_cast<int>(Layer::HighwayOverlay) && key.order==-1,
            "all multiplier children stay inside the source Combo group");
        check(before(renderer(17009),key) && before(key,renderer(15635)),
            "complete Combo group stays above neck and below rail");
    }
    check(before(renderer(18205),renderer(18156)) &&
        before(renderer(18156),renderer(18166)) &&
        before(renderer(18166),renderer(14860)),
        "multiplier background, numeral, frame and tick glow retain internal order");
    for(int id:{14855,17107,17172}) {
        const auto key=renderer(id);
        check(key.layer==static_cast<int>(Layer::TrackHUD) && key.order==3,
            "health arrow and both glows share their source group");
        check(before(renderer(17947),key) && before(renderer(14434),key),
            "health arrow stays above container and colored health band");
    }
    check(before(renderer(14855),renderer(17172)),"arrow sprite masks its glow within the group");
    const std::array<Key,5> input{{at(Layer::Notes),renderer(17997),at(Layer::KickOpenNotes),renderer(16093),renderer(18166)}};
    auto sorted=input;std::stable_sort(sorted.begin(),sorted.end(),before);
    check(sorted[0].layer==static_cast<int>(Layer::HighwayOverlay) && sorted[0].order==-1 &&
          sorted[1].layer==static_cast<int>(Layer::HighwayOverlay) && sorted[1].order==500 &&
          sorted[2].layer==static_cast<int>(Layer::KickOpenNotes) &&
          sorted[3].layer==static_cast<int>(Layer::Notes) &&
          sorted[4].layer==static_cast<int>(Layer::OverlayHUD),"overlap order follows PC layers before local order");
    std::puts("PASS: PC sorting groups, meter assembly order, rails, receptors and notes");
}
