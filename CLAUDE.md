# CISC187 Honors Contract — Traffic Flow Estimation via CV

## Project overview
- Honors Contract for Data Structures (C++). No fixed rubric from the professor — scope is self-set.
- Goal: estimate traffic flow from a single fixed-camera video using background subtraction and classical (non-deep-learning) CV, not off-the-shelf detectors.
- **Deadline: Dec 2, 2026** (self-imposed).
- Julian implements the data structures himself and understands their APIs deeply. Everything else (OpenCV, video I/O) can be third-party, but the APIs used should still be understood, not just called blindly.

## Scope

### Implementing myself (committed — at least 2)
- [ ] **BFS-based connected-components labeling** — extract vehicle blobs from the foreground mask (compare against `cv::findContours` as a sanity check)
- [ ] **Heap-based non-max suppression** — dedupe/filter overlapping blob detections

### Stretch (attempt if time allows; ask for help if crunched near the deadline)
- [ ] **Ring buffer** — fixed-size per-track history of recent centroids; smooths the noisy blob centroids into a velocity estimate (feeds position prediction and the per-vehicle velocity deliverable)
- [ ] **KD-tree** — nearest-neighbor matching for frame-to-frame vehicle tracking
- Considered, not planned: **union-find** for merging split tracks (see Open questions)

### Deliverables
- **Minimum:** vehicle counts crossing a defined line/region in frame
- **Stretch:** per-vehicle velocity in pixels/frame (from the ring buffer of recent centroids), then speed in world units (needs pixel↔world calibration), per-lane counts (needs lane geometry)
- **Benchmark:** pipeline counts vs. manually-counted ground truth on the same footage

### Explicitly third-party (not reimplementing)
- Background subtraction itself — OpenCV `BackgroundSubtractorMOG2`/`KNN`
- Video decode/I/O, basic image preprocessing (blur, morphological ops)

## Working style
- **Navigator mode by default:** Julian writes the code. Give detailed plain-English guidance and reviews (most important issues first, no rewrites). Only write code when explicitly asked ("show me", "write X for me"). Building and running tests to verify his code is welcome.

## Tech stack
- C++17, g++, VS Code (current setup — works fine, no need to change)
- **Two machines:** Windows (MSYS2 UCRT64) and macOS (Apple Silicon MacBook Air, Homebrew), set up Oct 9. Full install steps for both are in `README.md` → Setup.
  - Portable Makefile + README setup, morphology, and the `ccl.*` split were merged into `main` Oct 10 (merge `41bc4a9`) and pushed. Verified on Windows after the merge: `make` clean, headless `auckland-hwy` @0.5 = 15.9 s / 1500 frames (~10.6 ms/frame). Stale remote branches `cross-platform-setup`, `morphology`, `week3-docs` can be deleted.
- OpenCV 5.0.0 on **macOS** — `brew install openssl@4` first, then `brew install opencv` (Homebrew 7.0.9 self-lock bug on `openssl@3` otherwise; details in README). pkg-config name `opencv5`, headers `/opt/homebrew/opt/opencv/include/opencv5`, libs `/opt/homebrew/opt/opencv/lib` (Apple's linker doesn't search it by default, hence `--libs-only-L`). HighGUI works with no extra setup. `g++` is Apple clang 21.
  - Build shortcut on macOS is **Cmd+Shift+B**; `.vscode/tasks.json` runs plain `make` with `/opt/homebrew/bin` prepended to `PATH`.
  - Run with `build/main.exe ...` directly; no PATH/DLL setup needed.
  - Homebrew installs fail inside Claude Code's Bash sandbox (lock files), so they need to run unsandboxed or by Julian.
  - macOS headless timings (Oct 9, with BFS step, `-g` build): 47.7 ms/frame @1.0, 13.6 ms @0.5. Not comparable to the pre-BFS Windows numbers below.
- OpenCV 5.0.0 on **Windows** — installed via MSYS2 UCRT64 (`pacman -S mingw-w64-ucrt-x86_64-opencv`), verified Sep 30 with `scratch/hello_opencv.cpp`
  - pkg-config name is `opencv5`; headers in `C:/msys64/ucrt64/include/opencv5`
  - HighGUI (`imshow`) needs Qt6 at runtime: `mingw-w64-ucrt-x86_64-qt6-5compat` (pacman lists it as optional, but it isn't for us)
  - `C:\msys64\ucrt64\bin` is on Windows PATH so exes run outside the UCRT64 terminal
- ffmpeg — installed (came in as an OpenCV dependency)
- Build: `Makefile` at repo root (tracked), one rule per exe, output in `build/`. Default target `build/main.exe` links `core`, `highgui`, `videoio`, `video` (MOG2), `imgproc` (`resize`, `cvtColor`, `putText`) explicitly, with include/lib paths from `$(shell pkg-config --cflags opencv5)` and `--libs-only-L opencv5` (no hardcoded paths, so it works on both machines). `scratch/hello_opencv` still links everything via `pkg-config --libs opencv5`.
  - Ctrl+Shift+B (`.vscode/tasks.json`, gitignored) runs `make` through MSYS2 bash with `MSYSTEM=UCRT64`; setup details in `log.md` (9-30-26).
  - Julian keeps the Makefile deliberately simple: a single rule compiles every `.cpp` in one `g++` call (no `.o` files). The prerequisites list every `.cpp` **and every header** by hand (manual stand-in for `-MMD`), and the `g++` line names each `.cpp` explicitly (`$<` would drop files, `$^` would pass headers). Add new files to both lists (e.g. `ccl.cpp`/`ccl.hpp` in Week 3). Move to per-file `.o` rules / pattern rule + `-MMD -MP` only if rebuilds get slow or a forgotten header causes a stale build.
- Running exes from Git Bash needs `/c/msys64/ucrt64/bin` on `PATH` (otherwise they exit silently, missing DLLs).

## Code layout
- `src/main.cpp`: capture, MOG2 loop, display, key controls. Pipeline constants are named at the top of the anonymous namespace, above `process()` (MOG2 `history` 500, `varThreshold` 16, learning rate 0.003 passed as the third argument of `apply`; `blobAreaThreshold` 100; `iouThreshold` 0.5). Per frame: `process` -> threshold 200 -> open 3 / close 7 -> `label` -> erase-remove blobs under `blobAreaThreshold` -> `nms` -> `render` draws the kept blobs and a `Blobs: N` line (optional `std::optional<int>` parameter) above the frame counter.
- `src/args.hpp` / `src/args.cpp`: hand-rolled CLI parsing (Julian chose this over CLI11/cxxopts/`cv::CommandLineParser`). `InputFlags` struct (`video_path`, `headless`, `scale`, default 1.0), `argParse` throws `std::runtime_error` on any bad input (one catch in `main` prints the error + `printUsage`, returns 1). `getNextArg` is file-private (anonymous namespace).
- `src/ccl.hpp` / `src/ccl.cpp`: BFS connected-components labeling. Public: `Blob {bounding_box, area, centroid}` and `enum class Neighbors {Four, Eight}` and `label(mask, labels, blobs, Neighbors)` (binary `CV_8UC1` in, `CV_32SC1` labels out, 0 = background, blobs numbered from 1 in scan order; `--kneighbors <4|8>`, default 4). `floodFill` and the neighbor tables are file-private. `#ifndef NDEBUG` check at the end of `label()`: blob areas sum to `countNonZero(mask)`. `main` thresholds the MOG2 mask at 200 (drops shadows = 127), then runs morphology (open 3x3 → close 7x7, `openKernelSize`/`closeKernelSize` in processing-scale pixels) into `filterMask`, then calls `label(filterMask, ...)`; `render` shows `filterMask` and draws boxes with `area >= blobAreaThreshold` (display-only filter).
- `scratch/ccl_check.cpp` (`build/ccl_check.exe`, built by name): compares `label` against `cv::connectedComponentsWithStats` per frame. It links `src/ccl.cpp` and includes `../src/ccl.hpp` (no private copy, Oct 10). `KnTest` (4 or 8) drives both `label` and OpenCV's connectivity; blobs are sorted by (y, x, area, width, height) before comparing. Video path is hardcoded (relative to the repo root, so run it from there), `scale` is a constant, and it loads `auckland-fwy`.
- `src/heap.hpp` / `src/heap.cpp` (Oct 16, written by Julian): hand-written `BlobMaxHeap`, a max-heap of `Blob` copies ranked by area; ties go to the smaller `y`, then the smaller `x` (`aLTb` is file-private). Public: bulk-build constructor from `const std::vector<Blob>&` (O(n), `siftDown` from the last parent), `top()`, `pop()`, `empty()`, `size()`. `top`/`pop` on an empty heap are caller bugs (`assert`, active since the build has no `-DNDEBUG`). No `push`/`siftUp`; NMS doesn't need them.
- `src/nms.hpp` / `src/nms.cpp` (Oct 16-17, written by Julian): public `std::vector<Blob> nms(const std::vector<Blob>&, double iouThreshold)` builds a `BlobMaxHeap`, pops the largest blob, drops it if any kept box has IoU **strictly greater** than the threshold, else keeps it (output is ordered largest first; empty in = empty out). File-private `iouScore` (IoU of two `cv::Rect`, 0 for disjoint, touching or zero-union). `nms.hpp` has `#pragma once`. **Known limit:** IoU is intersection over union, so a small box nested inside a big one scores near 0 and is not suppressed; blobs are connected components (fragments of a vehicle), not duplicate detections. Decided Oct 21: keep plain IoU + the area filter (the intersection-over-smaller alternative was rejected as non-standard); `nms_check.cpp` stays as is.
- `scratch/nms_check.cpp` (`build/nms_check.exe`, run from the repo root, written by Claude): hand-made cases (empty, identical, disjoint, touching, the A-B-C chain, the threshold boundary at exactly 0.25, threshold 1.0, ordering) plus 2,000 random trials checking that survivors never overlap above the threshold, every dropped blob overlaps a survivor, and the largest blob comes first. All pass (Oct 16). It computes IoU independently via `cv::Rect`'s `&`.
- `scratch/mog2_tune.cpp` (`build/mog2_tune.exe`, written by Claude): `<video> [frame=336] [scale=0.5] [out=build/mog2_tune.png]`. Runs one MOG2 per entry of the `variants` table (varThreshold, history, blur, learning rate, warm-up frames, open/close kernel, 4 or 8 connectivity), replaying the clip from frame 1 so each model has its history, then renders the target frame's mask for each (threshold -> open -> close -> label, boxes for area >= 100) in a grid with blob counts. The table currently holds the 4 vs. 8 setup; the earlier tables (learning-rate sweep, kernel sweep) are not saved. Comparison images are in `assets/` (`Week 4 - ...`, gitignored).
- `scratch/warmup_check.cpp` (`build/warmup_check.exe`, written by Claude, Oct 21): `<video> [scale=0.5] [out_prefix=build/warmup]`. Runs the `main` pipeline (MOG2 at the fixed rate, threshold 200, open 3, close 7, label, area >= 100) for 300 frames and writes `<prefix>.csv` (frame, fg pixels, kept blobs for frames 1-240), `<prefix>_grid.png` (3 rows at frames 10/50/150: color frame left, mask right) and `<prefix>_plot.png` (fg pixels vs. frame with baseline, 1.5x limit and settling frame). Settling rule: baseline = median fg pixels over frames 150-300; a metric is settled at the first frame that stays <= 1.5x baseline for 30 frames in a row; warm-up = later of the fg-pixel and blob-count settle frames + 15 margin. The pipeline constants at the top are a manual copy of `main.cpp`'s, so keep them in sync. Results at 0.5: `auckland-hwy` settles at 97 (-> 112), `auckland-fwy` at 70 (-> 85). Outputs in `build/` (gitignored).
- `scratch/heap_check.cpp` (`build/heap_check.exe`, built by name, run from the repo root): builds `BlobMaxHeap` from random blobs (sizes 0–1000, 200 trials each, small value ranges so many ties) and pops everything, comparing against the same blobs sorted with an independent copy of the ranking rule and against `std::priority_queue` on area; also checks `size`/`empty`. 233,800 pops, 0 failures (Oct 16). It can't see the private vector, so it has no direct parent-vs-child check.
- Usage: `main.exe --video_path <file> [--headless] [--scale <s>]`, `0 < s <= 1`. Headless skips all display work and `waitKey` (no pause/quit; Ctrl+C only).
- Display at the default scale 1.0 is a 3840x1080 window (too wide); use `--scale 0.5` for interactive runs.

## Data
- Source: 6 Pexels clips downloaded by hand (Sep 30) into `source-videos/`. No trimming needed; all clips are 21–60 s. Two more `i5-*` clips (`i5-incoming` = `dev` role for BFS work) were added to `meta.json` Oct 9. Files use the Pexels ID filename (e.g. `16516296_1920_1080_25fps.mp4`), not the alias.
- **`source-videos/meta.json` is the source of truth** for per-clip metadata (URL, resolution/fps, camera stability, role, and placeholders for count line, mask regions, ground truth). Refer to clips by their alias there.
  - **Main:** `auckland-hwy` (16516296): 1080p25, 60 s, tripod-stable, dense traffic with some overlap
  - **Validation:** `auckland-fwy` (16516297): 1080p25, 90 s, tripod-stable, same creator as main (also Auckland), different framing. Tune on main, then check here without re-tuning.
  - **Backup:** `urban-road` (14350696): 4K60, 37 s, tripod-stable, sparse traffic (easy case). Downscale before processing.
  - Unused: `birdseye-4way` (drone drift/rotation), `motorway-vertical` (portrait, mild drift). Excluded: `skyline-hwy` (moving camera).
- License: Pexels License. Free to use and modify, no attribution required, but no redistributing unaltered copies. Raw `.mp4`s stay out of git (`*.mp4` in `.gitignore`); `meta.json` is tracked.

## Reporting
- Weekly progress report (informal, self + advisor updates) — every Sunday. Saved as `reports/week-NN.md`.
- Formal write-up at the end, for Julian's own record.
- Candidate automation: Claude Code Desktop's **local scheduled tasks** (Routines page, or `/schedule` in a session) — e.g. a weekly prompt that reads the week's commits/diffs and drafts a progress-report entry. Requires the Desktop app open and the machine awake to fire. If you want it to run even when your machine is off, use a **cloud routine** instead of a local task.

## Roadmap (Sept 24 → Dec 2)

| Week | Dates | Focus |
|---|---|---|
| 1 | Sep 22–28 | Env setup: install/verify OpenCV, confirm g++ links it ✅ (Sep 30); pull footage ✅ (Sep 30, 6 Pexels clips, see `meta.json`); git repo + this file ✅ |
| 2 | Sep 29–Oct 5 | Makefile + video playback/timing on `auckland-hwy` ✅ (Sep 30). MOG2 end-to-end + mask shown next to frame ✅ (Oct 1). Timing: headless pipeline is 13.2 ms/frame at 1080p, 4.5 ms at 540p; display was ~80% of interactive cost ✅. Pause/quit controls + frame counter ✅. Refactor (Oct 3–4): CLI flags `--video_path/--headless/--scale` in `src/args.*` ✅, display behind `!headless` ✅ (headless timings unchanged: 13.1 ms @1.0, 4.5 ms @0.5). Left: Step 2 of refactor (scale-aware `putText` position/font, skip `resize` when scale = 1.0), Step 3 (extract process/render/input functions; pass output Mats by non-const ref; key-code constants), measure warm-up frames. Pause/quit re-tested after refactor ✅; log ✅ + weekly report ✅ (`reports/week-02.md`, Oct 4) |
| 3 | Oct 6–12 | Implement BFS connected-components labeling → bounding boxes per blob. BFS labeling + blob boxes drawn + area-sum debug check ✅ (Oct 7, `src/ccl.*`, ~6 ms/frame at 540p). Morphology (open 3x3, close 7x7) before labeling ✅ (Oct 9). BFS vs. `cv::connectedComponentsWithStats` ✅ (Oct 9): 0 mismatches on `auckland-hwy` @0.5 and `auckland-fwy` @1.0 (boxes, areas, centroids). Cross-platform branches merged ✅ (Oct 10). `--kneighbors` flag ✅ (Oct 10). `ccl_check` now uses the shared `ccl.*` and matches OpenCV at both 4 and 8 on `auckland-fwy` @1.0 ✅ (Oct 10, 2255 frames, 0 mismatches). Left: 4 vs. 8 blob counts on the dense clip, re-add a min-blob-size filter outside `render`, check whether the 7x7 close merges two nearby cars (confirmed on `auckland-hwy`), finish `reports/week-03.md` (draft exists; send Sun Oct 11) |
| 4 | Oct 13–19 | Implement heap-based NMS; tune noise filtering (min blob size, morphology). Progress (Oct 16): `BlobMaxHeap` (`src/heap.*`) written and tested against a sorted reference and `std::priority_queue` ✅; IoU helper in `src/nms.cpp` ✅. `nms()` written, tested (`scratch/nms_check.cpp`) and wired into `main` after the min-blob-size filter ✅ (Oct 17). Week 3 carry-overs done ✅: min-blob-size filter (erase-remove, area < 100 at 0.5), 4 vs. 8 (identical at the 7x7 close; keep 4), close kernel (7 is best of none/3/5/7/9; 9 merges neighbours). MOG2 tuned ✅: fixed learning rate 0.003 (see Open questions); varThreshold, blur and `history` made little difference. Headless `auckland-hwy` @0.5 = 17.1 s / 1500 frames (~11.4 ms/frame). `reports/week-04.md` drafted (send Sun Oct 18). Closed Oct 21 ✅: NMS metric (keep plain IoU + area filter) and warm-up length (measured with `scratch/warmup_check.cpp`, ~112 frames) |
| 5 | Oct 20–26 | Naive centroid tracking + line-crossing counting → first end-to-end counts. Progress (Oct 21): Week 4 leftovers closed (NMS metric, warm-up ~112 frames). Next: `warmupFrames` constant in `main.cpp`, count-line geometry in `meta.json`, tracker (own track IDs, tolerate a few missed frames), line-crossing counter, `reports/week-05.md` |
| 6 | Oct 27–Nov 2 | Manual ground-truth count on the same clip; measure accuracy; iterate on misses/double-counts/occlusion |
| 7 | Nov 3–9 | Buffer/iteration on accuracy. **Decision point:** go/no-go on stretch structures based on time left. If the Week 6 error breakdown shows merged vehicles dominate, try **blob splitting** (marker-controlled watershed on the distance transform, or track-seeded multi-source BFS; see Open questions) and re-measure against the same ground truth |
| 8 | Nov 10–16 | Stretch: ring buffer of recent centroids (velocity, position prediction) and/or KD-tree tracking (if pursuing) — else polish + more test clips |
| 9 | Nov 17–23 | Stretch: per-vehicle velocity (px/frame) and speed estimation / per-lane counts (if time allows) |
| 10 | Nov 24–30 | Thanksgiving week, light load: formal write-up, code cleanup, finalize benchmark numbers |
| — | Dec 1–2 | Final polish, submit |

## Open questions
- ~~OpenCV version/install method not yet decided~~ → resolved Sep 30: OpenCV 5.0.0 via MSYS2 UCRT64 pacman
- ~~Confirm `BackgroundSubtractorMOG2` location/API in OpenCV 5~~ → resolved Sep 30: still `cv::createBackgroundSubtractorMOG2` (and `KNN`) in the `video` module, declared in `opencv2/video/background_segm.hpp` (`/ucrt64/include/opencv5/...`). Link with `-lopencv_video`.
- Exact line-crossing / counting-zone geometry not yet designed (store it in `meta.json` → `annotations.count_line`)
- ~~MOG2 shadow detection~~ → resolved Oct 1: `detectShadows = true`; threshold to a separate binary mask (only 255 = foreground) before BFS
- Downscale factor: **optional** for the pipeline (headless 1080p = 13.2 ms/frame, fits the 40 ms budget); needed only for interactive display. Keep as one variable `s` (`INTER_AREA`, before MOG2); decide 1.0 vs 0.5 in Week 4 on mask/blob accuracy. Timing table in `log.md`
- ~~Warm-up length~~ → measured Oct 21 with `scratch/warmup_check.cpp`: with the fixed rate 0.003 the first frames show ghosts of frame 1's vehicles (170k fg pixels at frame 10 vs. 10k for the auto rate). By the settling rule (<= 1.5x the frames-150-300 median for 30 frames in a row) `auckland-hwy` settles at frame 97 and `auckland-fwy` at 70; plus a 15-frame margin, use **~112 frames** (the larger). The estimate is soft: fg count also tracks traffic density, so the curve wobbles after the ghosts are gone. Judged enough (show the problem, plan around it). Count only post-warm-up frames, including in the Week 6 ground-truth count. To do: add a `warmupFrames` constant to `main.cpp`
- MOG2 learning rate (Oct 17): the automatic rate (-1) is `1 / min(2 * frames, history)` (from memory of OpenCV's source; consistent with the sweep), so very fast early and 0.002 after frame 250 at `history` 500. A slow fixed rate from frame 2 is what cut fragments on `auckland-hwy` (frame 336: 21 -> 13 blobs >= 100); "auto for 300 frames then fixed" reproduced the baseline. On `auckland-fwy` the gain was not clear (about equal counts). Judged by eye on 2-4 frames per clip, no ground-truth masks
- Merged vehicles: `auckland-fwy` frame 336 has the cement truck and the van in front touching in the mask, so one box; no close kernel separates them. Known limitation (undercounts)
- Shadows: confirmed discarded (400 frames: 22.1M shadow pixels (127), 0 after the threshold at 200). Whether MOG2 mislabels some shadows as foreground (255) is unchecked
- Count-line coordinates: scale down once at startup vs. scale results up per frame (`meta.json` stays full-res)
- ~~Display flag~~ → done Oct 4 (`--headless`). Still open: headless read-only run for decode's share; MOG2 on grayscale (see `log.md` 10-1-26)
- Optional: pass `cv::CAP_FFMPEG` to `VideoCapture` to skip GStreamer (noisy warnings on bad paths; makes the timed decoder explicit)
- Union-find (dropped from stretch Oct 18): only worth building if Week 6 shows split tracks (occlusion) causing double-counts. Simple alternative: fold the absorbed track into the other in a map, erase it, and OR the "counted" flags. The merge rule (short time gap, B starts near A's predicted position, optional direction/size checks) is the real decision logic; union-find would only be the bookkeeping
- Brute-force comparisons for the write-up (optional, Week 7 or 10): label propagation and DFS vs. the BFS labeling (peak queue/stack size, time), brute-force all-pairs NMS vs. the heap (synthetic thousands-of-boxes test), brute-force vs. KD-tree matching (synthetic scaling). Not in the original scope; check whether the advisor expects any
- Merged vehicles are a mask-stage problem, not an NMS one (NMS only picks among blobs that already exist). If Week 6 shows merges dominate the errors, options from cheapest up:
  - Area-based count: add `round(area / typical single-car area)` when a track crosses the line (crude; trucks break it)
  - **Marker-controlled watershed on the distance transform** (blob splitting): `cv::distanceTransform` on each large blob, keep pixels above ~50-70% of the blob's max as seeds (my `label()` can find them), then grow the seeds back over the blob with `cv::watershed` or my own multi-source BFS (cut falls midway between seeds). Gate it to blobs bigger than a typical car, require a minimum seed separation, and test at 1.0 as well as 0.5 (the neck may be only a few pixels at 0.5). Fails on truck+car (off-center cut), heavy overlap with no neck, and long vehicles (bus) over-split
  - **Track-seeded splitting**: when a blob contains 2+ predicted track positions (last centroid + ring-buffer velocity), run a multi-source BFS from those seeds. Keeps track identities through the overlap, but can't split cars that enter already merged and the seeds drift. Could fall back to the distance transform when a blob is too big but holds one seed
  - Evaluate against the Week 6 baseline on the same ground truth, and check `auckland-fwy` without re-tuning
- Step forward/back controls: forward is simple; back needs a circular buffer (MOG2 can't un-learn). Optional
