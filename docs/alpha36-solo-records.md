# Solo display, song records and menu input

PC references inspected through Ghidra MCP:

- Engine hit notification `0x1820F2790` calculates solo percentage from hit
  parents divided by the full solo note count using float32 truncation.
- `SoloCounter` `0x1802BA6F0` starts at zero; `0x1802BA820` changes the
  percentage only when its value changes. The resources.assets Solo Counter
  RectTransform is centered at `(0, 165)` with bounds `182 x 91`.
- `Countdown.Update` `0x180123730` counts the current gap to incoming notes.
  The existing converter `0x1820BAF50` produces gaps longer than ten seconds;
  this is not a dedicated solo-approach warning.
- The extracted song-select scene contains Score, Star Backgrounds and Stars.
  The native list now uses the extracted star sprites for its local records.

The solo label/percentage is inside each player's HUD viewport, before the
shared band score and global overlays. It is hidden when paused and outside the
solo range. Scoring and solo bonuses are unchanged.

Saved record version 3 stores score, star rank, accuracy, controller type and player name.
Version 2 records retain their known metadata without inventing accuracy.
Instrument/difficulty remain in the existing record filename. Atomic temporary
file + fsync + rename remains in use. Version 1 score-only records remain readable;
missing names/stars are not inferred. Failed runs, bots and practice do not save.
The list displays stars, accuracy and points on one horizontal row, with dark
stars when highlighted. Local Best starts with the instrument icon, player
and score. The list shows the highest-scoring saved run across instruments/difficulties,
and the same run's player/instrument/difficulty in the lower-right Local Best
panel. Rendering uses a cached record index, without per-row file reads.

Song-list previews use the main widget policy: dedicated preview files first,
otherwise a randomized full-stem mix up to 60 seconds, with a one-second fade-in
and a five-second fallback fade-out. A screen transition for the same loaded song
reuses the decoded excerpt. Dedicated preview endings remain authored.

Menu confirmation and action presses have a 220-ms guard; directional edges
retain the existing 75-ms guard. Held scrolling retains the PC .3-second delay
and .026-second accelerated repeat. Each player has independent history.
No note-judging or gameplay strum filter was changed.

As requested, Highway Effects uses steady blue coloring and the extracted
highway glow layers during solos. Star Power keeps priority over the glow
layers; danger keeps priority over the highway tint. The option remains off
unless enabled for that player. No new image or sound assets were introduced.

Console checks captured a two-note solo at SOLO 50% with the steady blue glow.
A bounded profile-menu test verified repeated Start/confirm edges are suppressed.
The saved-score UI capture was initially on the player-selection page, so its
visual validation needs a corrected song-list capture.
