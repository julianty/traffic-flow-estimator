# CISC187 Honors Project — Progress Log

## 2026-10-05 — Week 2 (Sep 29–Oct 5): Makefile + playback/timing, MOG2 end-to-end, pipeline refactor
**Status:** On track — every Week 2 deliverable is committed; only two small measurement/re-test items remain, and BFS CCL (Week 3) starts tomorrow as planned.

**Changes this week:**
- Repo bootstrap: progress log, OpenCV test file, roadmap updates (`9fca3c2`, `f9c00ff`, `6bc014b`, `43623ae`)
- `source-videos/meta.json` with per-clip metadata for all 6 Pexels clips (`480ff95`)
- Makefile + first build checkpoint + playback/timing tests on `auckland-hwy` (`738d10a`, `a8813e7`, `aab7941`)
- MOG2 linked (`-lopencv_video`) and working end-to-end, mask shown beside frame, shadow handling decided (`8cb0db2`, `46d5f07`); doc/README corrections + week-2 images (`a5b9407`, `b24f4fe`, `c997308`)
- Hand-rolled CLI (`--video_path`, `--headless`, `--scale`), moved to `src/args.{hpp,cpp}` (`165d542`, `317a757`)
- Log + CLAUDE.md update and weekly report `reports/week-02.md` (`9fac412`)
- Refactor Step 3: loop split into `process` / `render` / `handleInput`, `KeyAction` enum, key-code constants, outputs by non-const ref (`4fa131a`, `ded860f`, `0e06837`, `2c1dbf1`)
- Refactor Step 2: skip `resize` at scale 1.0 (headless 1080p 13.1 → ~12.7 ms/frame); frame counter font/thickness/position scaled from frame height via `getTextSize` (`dce2b26`)
- Uncommitted: `Makefile`, `scratch/hello_opencv.cpp`, `src/args.{cpp,hpp}`, `src/main.cpp` show as modified, but the diff is empty with `-w --ignore-cr-at-eol` — line-ending churn only, no code change

**Roadmap progress:**
- Week 1 (env, footage, repo): done — all ✅ in CLAUDE.md, `meta.json` committed
- Week 2 Makefile + playback/timing: done (`aab7941`, timings in `log.md`)
- Week 2 MOG2 end-to-end + side-by-side mask: done (`46d5f07`)
- Week 2 pause/quit + frame counter: done
- Week 2 refactor Step 1 (CLI flags, headless): done (`317a757`)
- Week 2 refactor Step 2 (scale-aware `putText`, skip resize at 1.0): done (`dce2b26`) — CLAUDE.md roadmap row still lists it as "Left"
- Week 2 refactor Step 3 (process/render/input functions, key constants): done (`ded860f`) — CLAUDE.md row still lists it as "Left"
- Week 2 measure warm-up frames: not started (still in `log.md` Open list)
- Week 2 re-run display-mode playback list after the `handleInput` change: partial — counter checked at 0.5/1.0, full pause/quit list (esp. space → other key → stays paused) not re-run
- Week 2 log + weekly report: done (`reports/week-02.md`)
- Week 3 BFS CCL: not started — not due until Oct 12

**Behind schedule?** No. Warm-up measurement and the playback re-test are ~30 min of leftover; fold them into the first Week 3 session (warm-up N matters before counting in Week 5, not before CCL).

**Next week:** Week 3 (Oct 6–12) — implement BFS connected-components labeling on the thresholded MOG2 mask → bounding boxes per blob (add `ccl.cpp`/`ccl.hpp` to both Makefile lists; draw boxes only on the display `frame`, never on `imgScaled`/`mask`).

## 2026-10-12 — Week 3 (Oct 6–12): BFS connected-components labeling → bounding boxes per blob
**Status:** On track — BFS CCL is in the pipeline, split into `src/ccl.*`, and matches `cv::connectedComponentsWithStats` exactly at 4- and 8-connectivity; what's left is small tuning that overlaps Week 4's "tune noise filtering" item, plus the unsent week-03 report.

**Changes this week:** (21 commits, all on `main` and pushed; no uncommitted changes)
- BFS step added to the pipeline, then the flood fill refactored (`8b62af5`, `c5d3a5b`)
- Cross-platform build (Windows MSYS2 + macOS Homebrew) with README setup steps; CLAUDE.md macOS notes (`7a65472`, `1637151`, `645d1a3`)
- Morphology before labeling: open 3x3 → close 7x7 (`273adae`)
- i5 clip metadata in `meta.json`, week-03 report draft (`2ffd2cd`)
- `scratch/ccl_check.cpp`: BFS vs. `connectedComponentsWithStats` harness — 0 mismatches over 3,755 frames on both Auckland clips (`658ce3f`)
- CCL moved into `src/ccl.cpp`/`ccl.hpp`, added to the Makefile (`fd25020`, `0420f4e`)
- Merged `origin/week3-docs` (Mac and Windows branches) into `main`, 3 conflicts resolved by hand; Windows re-verified at ~10.6 ms/frame @0.5 (`41bc4a9`, `08753de`); stale remote branches are gone (only `origin/main` remains)
- `--kneighbors <4|8>` flag, default 4; display-only area filter removed from `render` (`b90e5fd`, `906c207`)
- `ccl_check` now links the shared `ccl.*` (no private copy); 0 mismatches at both 4 and 8 on `auckland-fwy` @1.0 (2,255 frames) after adding sort tie-breakers (`90a2758`)
- Bookkeeping: log entries, week-03 report updates, `assets/` gitignored (`6c5524c`, `6cd99dd`, `93d7bb0`, `a5df091`, `4856d31`, `c587439`)
- Untracked: an empty `-p/` directory at the repo root (Oct 10, likely from a `mkdir -p` that ran under `cmd`); harmless, can be removed

**Roadmap progress:**
- Week 3 BFS labeling + blob boxes + area-sum debug check: done (`8b62af5`, `fd25020`)
- Week 3 sanity check against OpenCV: done, stronger than planned — `connectedComponentsWithStats` instead of `findContours`, exact match on boxes/areas/centroids at 4 and 8 (`658ce3f`, `90a2758`). Gaps: edge-touching blobs and the i5 clip not specifically covered
- Week 3 morphology: done (`273adae`)
- Week 3 4 vs. 8 connectivity: partial — flag done and verified; per-frame blob-count comparison on `auckland-hwy` not done (`ccl_check`'s "Total blobs" is last-frame only, so it can't answer this yet)
- Week 3 min-blob-size filter: not started — removed from `render` in `b90e5fd`, not re-added anywhere, so no stage currently filters tiny blobs
- Week 3 check whether the 7x7 close merges nearby cars: done as a finding — merging is confirmed on `auckland-hwy` (screenshots in `assets/`); fix not started
- Week 3 `reports/week-03.md`: partial — draft is current through Oct 10 but still says "Update this before sending on Sunday"; the Sun Oct 11 send date has passed
- Week 2 carry-over, measure warm-up frames: not started (still under Open questions in CLAUDE.md)
- CLAUDE.md Code layout still says `render` draws boxes with `area >= blobAreaThreshold`, which `b90e5fd` removed

**Behind schedule?** No on the core milestone. Late items are small: the week-03 report (due Oct 11) and the warm-up measurement (Week 2). Catch-up: finalize and send the report today, then make the first Week 4 session the min-blob-size filter as a real pipeline stage plus a per-run average/max blob count at k=4 vs. k=8 on `auckland-hwy` — both feed directly into NMS and noise tuning. Merged cars are the real risk: NMS dedupes overlapping boxes but won't split one merged blob, so budget time for a smaller close kernel or a split step this week, not later.

**Next week:** Week 4 (Oct 13–19) — implement heap-based NMS; tune noise filtering (min blob size, morphology kernels, shadow handling) and decide 1.0 vs. 0.5 processing scale.

## 2026-10-19 — Week 4 (Oct 13–19): Heap-based NMS; tune noise filtering (min blob size, morphology)
**Status:** On track — both committed data structures (BFS CCL, heap NMS) are now written, tested and in the pipeline, and all three Week 3 carry-overs are closed; the open items are one design decision (NMS metric) and a measurement that has slipped since Week 2 (warm-up length).

**Changes this week:** (10 commits, all dated Oct 16, on `main` and pushed — `main` is level with `origin/main`)
- `BlobMaxHeap` (`src/heap.*`, O(n) bulk build, area-ranked with y/x tie-breaks) and the IoU helper (`ab6aa11`)
- `scratch/heap_check.cpp`: heap vs. sorted reference and `std::priority_queue` — 233,800 pops, 0 failures (`faad383`)
- `nms()` in `src/nms.*`: pop largest, drop if IoU > threshold with any kept box (`d157793`)
- `scratch/nms_check.cpp`: hand-made edge cases + 2,000 random trials, all pass (`223eea0`)
- Min-blob-size filter (erase-remove, area < 100 @0.5) and `nms()` wired into `main`; `Blobs: N` overlay (`cc3a574`)
- `scratch/mog2_tune.cpp` side-by-side mask comparison tool, then per-variant kernels and 4/8 connectivity (`4d8f19b`, `8a08dd0`)
- Fixed MOG2 learning rate 0.003 passed to `apply()` (frame 336 on `auckland-hwy`: 21 → 13 blobs ≥ 100) (`b4b0129`)
- Log, CLAUDE.md and `reports/week-04.md` draft (`3436c5b`, `cd60a64`)
- Uncommitted: `CLAUDE.md` — ~13 real lines once line-ending churn is ignored (the raw diff shows 220 lines): union-find dropped from stretch and replaced by a ring buffer of centroids, velocity in px/frame added as a stretch deliverable, Week 7–9 roadmap rows rewritten (blob-splitting option in Week 7), new Open questions on merged vehicles/watershed and brute-force comparisons
- Still untracked: the empty `-p/` directory at the repo root (noted last week)

**Roadmap progress:**
- Week 4 heap-based NMS: done — heap + `nms()` written by Julian, tested, wired in (`ab6aa11`, `d157793`, `cc3a574`). Caveat: plain IoU doesn't suppress a fragment box nested inside a vehicle box, so NMS currently removes little on real frames; metric decision (IoU vs. intersection-over-smaller) **not made**
- Week 4 min blob size: done (`cc3a574`)
- Week 4 morphology tuning: done — close 7 chosen over none/3/5/7/9; 4 vs. 8 identical at close 7, keep 4 (`8a08dd0`, images in `assets/`)
- Week 4 MOG2 tuning: done — rate 0.003 fixed; varThreshold/blur/history made little difference; gain not confirmed on `auckland-fwy` (about equal counts)
- Week 4 1.0 vs. 0.5 processing scale (listed in last week's "Next week"): not decided — all tuning was done at 0.5; no 1.0 comparison recorded
- Week 4 `reports/week-04.md`: partial — draft is thorough but still contains "Update this before sending on Sunday"; the Sun Oct 18 send date has passed. It also still lists "confirm the Week 3 report was sent"
- Week 3 carry-overs (4 vs. 8 counts, min-blob filter, close-kernel merging): done this week
- Week 2 carry-over, measure warm-up frames: partial — ghosts seen through ~frame 60, gone by 100 on `auckland-hwy`; exact N between 60 and 100 not measured. Now three weeks old
- CLAUDE.md Scope checklist: both committed items (BFS CCL, heap NMS) are still `[ ]` although both are implemented and tested

**Behind schedule?** No on the milestones. Small items are late: the Week 4 report (due Oct 18), the warm-up measurement (Week 2), and the NMS metric decision. Catch-up: before starting the tracker, spend one session doing three things — finalize and send `week-04.md`, run `main` on `auckland-hwy` while logging fg-pixel count per frame for frames 60–100 to fix N, and pick the NMS metric (if nested fragments are what the tracker will choke on, try intersection-over-smaller and update `nms_check.cpp`'s expected values). Then commit the pending CLAUDE.md roadmap changes so the Week 7–9 plan in git matches the one you're working from.

**Next week:** Week 5 (Oct 20–26) — naive centroid tracking + line-crossing counting → first end-to-end counts. The count-line geometry is still undesigned (`meta.json` → `annotations.count_line`), so define it first; tracks should tolerate a few missed frames because blobs near the area threshold flicker.

## 2026-10-26 — Week 5 (Oct 20–26): Naive centroid tracking + line-crossing counting → first end-to-end counts
**Status:** On track — the tracker and line-crossing counter are written and running in `main`, so the pipeline produces a crossing count end to end. But that count has never been checked against anything, and that check is all of Week 6.

**Changes this week:** (11 commits, Oct 21–24, all on `main`; `main` is level with `origin/main`)
- Week 4 leftovers closed: `scratch/warmup_check.cpp` measures MOG2 settling (`auckland-hwy` 97 → 112 frames, `auckland-fwy` 70 → 85) (`3d650fb`). NMS metric decision recorded: keep plain IoU + the area filter (`efe11e5`)
- `warmupFrames` constant in `main.cpp` (`2c39e00`). Count line drawn in the render path (`e88906d`), `auckland-hwy` count line set to y = 200 in `meta.json` (`e791550`)
- `scratch/frame_grid.cpp` helper, written by Sonnet (`5cd7968`)
- `src/meta.*`: `loadClipMeta()` reads the clip's count line from `meta.json` by file basename, and `main` scales it by `--scale` (`7afa1f8`)
- `src/tracker.*`: greedy nearest-first centroid matching (100 px gate), 3 hits to confirm a track, dropped after 5 misses, counts once per track on a side change, counting only after warm-up. `Crossings: N` and a track overlay (ID + dot) added to `render` (`f5e4bb6`). The same commit raised `blobAreaThreshold` from 100 to 200, but CLAUDE.md's Code layout still says 100
- Log, CLAUDE.md and `reports/week-05.md` draft (`af02359`, `a2d68c6`, `2e9fde6`)
- **Uncommitted (staged):** `scratch/make_demo.cpp`, which records the overlay to an .mp4 (frames 121–400 by default; outputs `build/demo.mp4`/`.gif` already exist), plus its `make_demo` Makefile rule. The staged Makefile diff shows all 37 lines rewritten because the line endings changed to CRLF. The real change is only the new 5-line rule

**Roadmap progress:**
- Week 5 naive centroid tracking: done (`f5e4bb6`, written by Julian)
- Week 5 line-crossing counting: done for `auckland-hwy` (`7afa1f8`, `f5e4bb6`)
- Week 5 first end-to-end counts: partial. The count is computed but only shown on screen in interactive mode. The headless run prints only the timings (`main.cpp` ~L281), so no count has been recorded yet. The demo recorder suggests the overlay has been watched, but the logs don't say whether IDs stayed stable or vehicles were counted once
- Week 5 `auckland-fwy` count line: not started (`count_line` is still `null`), so the validation clip can't go through the tracker yet
- Week 5 `reports/week-05.md`: partial. It still says "Update this before sending on Sunday", and Sun Oct 25 has passed
- Earlier weeks: nothing outstanding. The Week 2 warm-up measurement and the Week 4 NMS metric decision were closed this week
- CLAUDE.md Scope checklist: BFS CCL and heap NMS are still `[ ]`, although both have been implemented and tested since Week 4

**Behind schedule?** No. The small late items are the headless count printout, the `auckland-fwy` count line and the Week 5 report. Catch-up: before starting the ground truth, spend about 30 minutes adding `std::cout << "Crossings: " << crossingTracker.count()` after the loop. Then run headless `auckland-hwy` @0.5 to get the first number and put it in `week-05.md` before sending. Week 6 compares against that number, so it has to exist first.

**Next week:** Week 6 (Oct 27–Nov 2): manual ground-truth count on `auckland-hwy` (post-warm-up frames only, frame > 120, same y = 200 line), then measure accuracy and sort the errors into misses, double-counts (split vehicles) and merges (occlusion). Use `build/demo.mp4` to tag each error. Commit `make_demo` first, and consider normalizing the Makefile's line endings back to LF in the same commit.

## 2026-11-02 — Week 6 (Oct 27–Nov 2): Manual ground-truth count on the main clip; measure accuracy; iterate on misses/double-counts/occlusion
**Status:** Behind — Week 6 ends today and its core milestone (manual ground truth → accuracy number) hasn't started. The week went into new footage and switching the main clip twice, and no pipeline code changed.

**Changes this week:** (6 commits, Oct 26–Nov 1, all on `main`. `bc2dbf0` is **not pushed**: `origin/main` is at `4be56ba`. Working tree clean, nothing uncommitted.)
- `16th-st` / `16th-st-2` own iPhone clips added to `meta.json`, plus `scratch/stabilize.py` (ORB + RANSAC frame locking) (`003d487`)
- `make_demo` committed: records the pipeline overlay to .mp4 and prints the crossing count (`2b72449`, last week's staged work). Week 6 checkpoint GIFs made for `ucsd-i5-incoming`/`-outgoing`
- Main/validation switched from the Auckland Pexels clips to Julian's own 60 fps UCSD I-5 footage: `ucsd-i5-incoming` (y = 550, ~76 crossings) and `ucsd-i5-outgoing` (y = 750, ~100). Count lines were picked by running the tracker against candidate lines (`f2df932`)
- Week 5 progress-log entry (`f84f57f`). Docs swept for stale facts after the switch: CLAUDE.md, README, log.md backfilled for Oct 21/26/31, and both Scope checklist items are now ticked (`4be56ba`)
- Main switched again to the steadier DSLR take `ucsd-i5-incoming-dslr` (drift ≤ 2 px, y = 400, ~73 crossings, warm-up ~16 frames). `-dslr2`, `ucsd-i5-incoming` and `ucsd-i5-outgoing` are now validation (`bc2dbf0`, unpushed)
- No changes to `src/` this week

**Roadmap progress:**
- Week 6 manual ground-truth count on main: not started. Every `ground_truth` entry in `meta.json` is still null, and log.md lists it as "Open" for `ucsd-i5-incoming-dslr`
- Week 6 measure accuracy: not started (needs the ground truth). The ~73 / ~66 / ~76 / ~100 figures are tracker output, unverified
- Week 6 iterate on misses/double-counts/occlusion: not started. No tracker or mask changes. The 60 fps concern (`maxMissed` 5 = 83 ms) was noted but not tested
- Week 6 `reports/week-06.md`: not started (the file doesn't exist; `reports/` stops at week-05)
- Week 5 carry-over, print the count in headless mode: still not done. `main.cpp` only prints timings (~L282). `make_demo` prints a count, so numbers are available that way
- Week 5 carry-over, watch the overlay for stable IDs / one count per vehicle: partial. GIFs were made and some artifacts noted (top-right box, edge streak), but no per-vehicle check is recorded
- Week 5 carry-over, `auckland-fwy` count line: moot after the clip switch
- Re-tune MOG2/kernels/area filter on 60 fps footage (raised in log.md Oct 31): not started. `mog2_tune` and `warmup_check` still use area 100 vs. `main`'s 200

**Behind schedule?** Yes, by about one week. Late: the Week 6 ground truth, the accuracy breakdown, the report, and the Week 5 headless count. Week 7 (Nov 3–9) is already the buffer week, so it absorbs this, but the stretch go/no-go decision depends on the error breakdown. Catch-up: freeze the main clip at `ucsd-i5-incoming-dslr` (no more switching). Add the one-line `Crossings:` print to headless `main`, then count by hand on frames > 120 at y = 400. At 49.5 s and ~73 vehicles that's one sitting, using `make_demo` frame ranges at 0.5× playback. Tag each disagreement as miss, double-count or merge, record the result in `meta.json` and `reports/week-06.md`, then push `bc2dbf0`.

**Next week:** Week 7 (Nov 3–9): buffer and iteration on accuracy. Decision point: go/no-go on the stretch structures (ring buffer, KD-tree) based on time left. If merged vehicles dominate the error breakdown, try blob splitting (distance-transform watershed or track-seeded multi-source BFS) and re-measure against the same ground truth.
