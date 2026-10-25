# Week 5 progress report (Oct 20 – Oct 26, 2026)

**Status: on track.** Week 5's goal was naive centroid tracking and line-crossing counting. Both are written and wired into the pipeline, and a track overlay (ID plus marker) is drawn on the video. I have not yet compared the counts to anything; that is Week 6. Update this before sending on Sunday with whatever the first run shows.

## Done this week

- **Warm-up length measured** (Week 4 carry-over, Oct 21): with the fixed learning rate, `auckland-hwy` settles at frame 97 and `auckland-fwy` at 70, so I use 120 frames (the 112 estimate plus a little room). Counting is switched off until then.
- **NMS metric decided:** keep plain IoU plus the area filter. The intersection-over-smaller alternative is non-standard, so I'm not using it.
- **Count line from metadata:** the line is read from `source-videos/meta.json` per clip, scaled to the processing scale at startup, and drawn on both panels. The loader throws on an unknown clip or a missing line.
- **Tracker (my own code, `src/tracker.*`):** each frame it pairs every track with every blob within 100 px, sorts the pairs by distance, and accepts them nearest-first if neither side is taken. Matched tracks update their position; unmatched tracks count a miss and are dropped after 5; unmatched blobs start new tracks with a fresh ID.
- **Line-crossing counter:** a track counts once, when the side of the line its centroid is on changes between the previous and current frame, and only after it has been matched 3 times. The side comes from the sign of a cross product.
- **On-screen output:** a `Crossings: N` line and, for every track, a circle at the centroid with its ID (yellow until counted, green after).

## Key findings

- **Most of the bugs were in the bookkeeping, not the idea.** In review I caught uninitialized loop counters, a "blob used" flag set at the wrong moment (it made every match fail), an inverted crossing test, and a missing `missed` increment (tracks would never have died). None would have shown up as a crash; they would have looked like "the count is wrong".
- **Design limits I already know about:** the crossing test treats the line as infinite; a crossing that happens before a track is confirmed is lost; a track missed for several frames keeps its old position, so a fast vehicle can come back as a new track; and a vehicle split into two blobs becomes two tracks and can count twice.

## Still open

- First end-to-end run: do IDs stay on one vehicle, does each vehicle turn green once, and is the count in the right range?
- A count line for `auckland-fwy`.
- Print the final count in headless mode.

## Next week (Week 6, Oct 27 – Nov 2)

- Manual ground-truth count on `auckland-hwy`, counting only after the warm-up frames.
- Measure accuracy and break the errors down: double counts (split vehicles), misses (merged vehicles, lost tracks).
- Iterate on the biggest error source; check `auckland-fwy` without re-tuning.

## Risks / open questions

- If split and merged vehicles dominate the errors, I'll need the Week 7 blob-splitting work, which is more than the original plan.
- `maxMatchDistance`, `maxMissed` and `minHits` are untuned guesses; I'll tune them against ground truth, not by eye.
