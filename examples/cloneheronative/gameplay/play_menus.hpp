#pragma once
#include <cstdint>
#include "feature_availability.hpp"
#include "pc_six_fret.hpp"
namespace rhythm::play_menu {
// PlayerSelectionMenu.SelectionMenuType, recovered from the PC DLL/level1.
enum class Page { Confirm, Instrument, Difficulty, Modifiers, Ready };
inline constexpr const char* instruments[]{"Lead Guitar","Bass Guitar","Rhythm Guitar","Drums","6-Fret Lead","6-Fret Bass","6-Fret Rhythm","6-Fret Co-op"};
inline constexpr const char* tracks[]{"Single","DoubleBass","DoubleRhythm","Drums","GHLGuitar","GHLBass","GHLRhythm","GHLCoop"};
inline constexpr const char* difficulties[]{"Easy","Medium","Hard","Expert"};
inline constexpr const char* modifiers[]{"All Strums","All HOPO's","All Taps","All Opens","HOPO's to Taps","Mirror Mode","No Fret Ghosting","Precision Mode"};
inline constexpr const char* confirm[]{"Ready","Instrument","Difficulty","Modifiers"};
// PauseMenu.menuStrings and practiceMenuStrings, serialized in PC level2.
inline constexpr const char* quickplay[]{"Resume","Restart","New Song","Practice","Options","Song Settings","Quit"};
inline constexpr const char* practice[]{"Resume","Restart Section","Set A Position","Set B Position","Clear A/B","New Section","Quickplay","New Song","Options","Quit"};
// Unity HorizontalLayoutGroup: MiddleCenter, child roots 100px wide,
// spacing 370px. Their 450px artwork leaves 20px between adjacent cards.
inline constexpr float cardLeft(int ordinal,int activeCount) {
    return 960.0f+(ordinal-(activeCount-1)*0.5f)*470.0f-225.0f;
}
inline constexpr int rows(Page page) {
    return page==Page::Confirm?4:page==Page::Instrument?8:page==Page::Difficulty?4:page==Page::Modifiers?8:0;
}
inline constexpr bool supported(int instrument,uint8_t mask,int difficulty) {
    return instrument>=0 && instrument<8 && difficulty>=0 && difficulty<4 && (mask&(1u<<difficulty));
}
inline constexpr uint8_t toggleModifier(uint8_t mask,int row) {
    // The four forced note types are mutually exclusive (PC modifier groups).
    const bool enabling=(mask&(1u<<row))==0;
    if(enabling && row<4) mask &= ~uint8_t(15);
    return mask ^ uint8_t(1u<<row);
}
}
