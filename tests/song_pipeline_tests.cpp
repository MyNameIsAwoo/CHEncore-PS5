#include "gameplay/guitar_session.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>

void check(bool value) {
    if (!value) { std::fputs("Pipeline check failed\n", stderr); std::exit(1); }
}

int main() {
    // Bot catches all due notes in a slow frame at zero timing error, retains
    // future notes, and does not repeat hits or score ignored practice notes.
    rhythm::SongChart botChart{{{1.,0.,1},{1.01,.5,3},{1.02,0.,0},{2.,0.,16}}};
    rhythm::GuitarSession bot(botChart);
    check(bot.updateBot(.99).empty());
    const auto botHits=bot.updateBot(1.5);
    check(botHits.size()==3 && bot.hits()==3 && bot.misses()==0 && bot.overstrums()==0);
    for(const auto& hit:botHits) check(hit.error==0.0);
    check(bot.updateBot(1.8).empty());
    check(bot.updateBot(2.5).size()==1 && bot.hits()==4);
    bot.beginSection(1.02);
    check(bot.updateBot(1.5).size()==1 && bot.hits()==1);
    bot.reset();
    check(bot.updateBot(2.5).size()==4);
    // A held strum cannot consume subsequent notes; releasing and pressing can.
    rhythm::SongChart chart{{{1., 0., 1}, {1.05, 0., 1}, {2., 0., 3}}};
    rhythm::GuitarSession session(chart);
    check(session.update({1, false, true}, 1.).has_value());
    check(!session.update({1, false, true}, 1.05).has_value());
    session.update({1, false, false}, 1.05);
    check(session.update({1, true, false}, 1.05).has_value());
    session.update({1, false, false}, 2.);
    check(!session.update({1, false, true}, 2.).has_value());
    check(session.update({3, false, false}, 2.).has_value());
    // The nearby wrong-fret strum is buffered and corrected, as on PC.
    check(session.hits() == 3 && session.overstrums() == 0);

    // Reconnection with a held strum is primed, not interpreted as an edge.
    rhythm::GuitarSession reconnected(chart);
    reconnected.prime({1, false, true});
    check(!reconnected.update({1, false, true}, 1.).has_value());

    rhythm::SongChart one{{{1., 0., 1}}};
    rhythm::InputEvaluator evaluator(one);
    auto window = [](std::size_t, const rhythm::Note&) { return rhythm::HitWindow{.125,.125}; };
    check(evaluator.eligibleNotes(.875, window).size() == 1);
    check(evaluator.expireNotes(1.125, window).empty());
    check(evaluator.expireNotes(1.126, window).size() == 1);

    std::size_t submitted = 0;
    rhythm::submitVisibleNotes(chart, 0., 1.1, .25,
        [&](const rhythm::RenderNote&) { ++submitted; });
    check(submitted == 2);
    rhythm::AudioClock clock({});
    clock.reset(1.);
    clock.update(.5, std::nullopt, false, false);
    check(clock.gameplayTime() == 1.);
    clock.update(.5, std::nullopt, true, false);
    check(clock.gameplayTime() == 1.5);

    // Dense-note recovery selects the closer matching head while consuming one strum.
    rhythm::SongChart dense{{{1.,0.,1},{1.05,0.,1}}};
    rhythm::GuitarSession ordered(dense);
    auto first = ordered.update({1,true,false},1.04);
    check(first && first->noteIndex == 1);
    ordered.reset();
    check(ordered.hits() == 0 && ordered.misses() == 0);
    ordered.prime({1,true,false});
    check(!ordered.update({1,true,false},1.04));

    // Held sustains stay visible after their head leaves the visibility interval.
    rhythm::SongChart sustain{{{1.,2.,1}}};
    submitted = 0;
    rhythm::submitVisibleNotes(sustain, 2., .5, .1,
        [&](const rhythm::RenderNote& n) { check(n.timeUntilHit == -1.); ++submitted; });
    check(submitted == 1);

    // Separate clock offsets and the exported gradual audio correction shape.
    rhythm::ClockConfig config;
    config.gameplayOffset = .1; config.renderOffset = .2;
    rhythm::AudioClock calibrated(config);
    calibrated.reset(1.);
    check(std::abs(calibrated.gameplayTime() - .9) < 1e-9);
    check(std::abs(calibrated.renderTime() - 1.2) < 1e-9);
    calibrated.update(.1, 1.2, true, true);
    check(std::abs(calibrated.gameplayTime() - 1.01) < 1e-9);
    const double before = calibrated.renderTime();
    calibrated.update(-1., std::nullopt, true, false);
    check(calibrated.renderTime() == before);
    config.scale = 0;
    check(!rhythm::AudioClock(config).valid());

    // Expiration may skip completed prefixes but must retain an older pending
    // head with a much wider custom window, even after later heads expire.
    rhythm::SongChart varied{{{1.,0.,1},{2.,0.,1},{3.,0.,1}}};
    rhythm::InputEvaluator custom(varied);
    auto customWindows=[](std::size_t i,const rhythm::Note&){return rhythm::HitWindow{.125,i==0?10.:.125};};
    check(custom.expireNotes(4.,customWindows)==std::vector<std::size_t>({1,2}));
    check(custom.nextPendingNote()==0);
    check(custom.judgeAutomatic(0,rhythm::NoteState::Hit));
    check(!custom.nextPendingNote());
    custom.reset();custom.ignoreNotesBefore(2.);
    check(custom.nextPendingNote()==1);
    check(custom.judgeAutomatic(2,rhythm::NoteState::Hit) && custom.nextPendingNote()==1);
    check(custom.missNotesBefore(2)==1 && !custom.nextPendingNote());
    custom.reset();check(custom.nextPendingNote()==0);
    // A long future chart must not invoke per-note judgement callbacks on
    // every frame; explicit bounds retain the same eligible note at a boundary.
    rhythm::SongChart longChart;
    for(unsigned i=0;i<50000;++i)longChart.notes.push_back({1.+i*.1,0.,1});
    rhythm::InputEvaluator bounded(longChart);unsigned visits=0;
    auto counted=[&](std::size_t,const rhythm::Note&){++visits;return rhythm::HitWindow{.07,.07};};
    check(bounded.expireNotes(0.,counted).empty() && visits==0);
    const auto boundedFirst=bounded.eligibleNotes(1.,counted,.07);
    check(boundedFirst==std::vector<std::size_t>({0}) && visits==1);
    // Invalid chart data is rejected without throwing or indexing state storage.
    rhythm::SongChart invalid{{{1.,0.,32}}};
    check(!rhythm::validateChart(invalid));
    rhythm::InputEvaluator rejected(invalid);
    check(rejected.eligibleNotes(1.,window).empty());
    invalid.notes[0].frets = 1;
    invalid.notes[0].time = std::numeric_limits<double>::quiet_NaN();
    check(!rhythm::validateChart(invalid));
    std::puts("Exported gameplay pipeline checks passed");
}
