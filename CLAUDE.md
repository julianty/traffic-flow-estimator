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
- [ ] **Union-find** — merging track associations / components across frames
- [ ] **KD-tree** — nearest-neighbor matching for frame-to-frame vehicle tracking

### Deliverables
- **Minimum:** vehicle counts crossing a defined line/region in frame
- **Stretch:** per-vehicle speed estimate (needs pixel↔world calibration), per-lane counts (needs lane geometry)
- **Benchmark:** pipeline counts vs. manually-counted ground truth on the same footage

### Explicitly third-party (not reimplementing)
- Background subtraction itself — OpenCV `BackgroundSubtractorMOG2`/`KNN`
- Video decode/I/O, basic image preprocessing (blur, morphological ops)

## Working style
- **Navigator mode by default:** Julian writes the code. Give detailed plain-English guidance and reviews (most important issues first, no rewrites). Only write code when explicitly asked ("show me", "write X for me"). Building and running tests to verify his code is welcome.

## Tech stack
- C++17, g++, VS Code (current setup — works fine, no need to change)
- OpenCV 5.0.0 — installed via MSYS2 UCRT64 (`pacman -S mingw-w64-ucrt-x86_64-opencv`), verified Sep 30 with `scratch/hello_opencv.cpp`
  - pkg-config name is `opencv5`; headers in `C:/msys64/ucrt64/include/opencv5`
  - HighGUI (`imshow`) needs Qt6 at runtime: `mingw-w64-ucrt-x86_64-qt6-5compat` (pacman lists it as optional, but it isn't for us)
  - `C:\msys64\ucrt64\bin` is on Windows PATH so exes run outside the UCRT64 terminal
- ffmpeg — installed (came in as an OpenCV dependency)
- Build: `Makefile` at repo root (tracked), one rule per exe, output in `build/`. Default target `build/main.exe` links `core`, `highgui`, `videoio`, `video` (MOG2), `imgproc` (`resize`, `cvtColor`, `putText`) explicitly. `scratch/hello_opencv` still links everything via `pkg-config --libs opencv5`.
  - Ctrl+Shift+B (`.vscode/tasks.json`, gitignored) runs `make` through MSYS2 bash with `MSYSTEM=UCRT64`; setup details in `log.md` (9-30-26).
  - Julian keeps the Makefile deliberately simple: a single rule compiles every `.cpp` in one `g++` call (no `.o` files). The prerequisites list every `.cpp` **and every header** by hand (manual stand-in for `-MMD`), and the `g++` line names each `.cpp` explicitly (`$<` would drop files, `$^` would pass headers). Add new files to both lists (e.g. `ccl.cpp`/`ccl.hpp` in Week 3). Move to per-file `.o` rules / pattern rule + `-MMD -MP` only if rebuilds get slow or a forgotten header causes a stale build.
- Running exes from Git Bash needs `/c/msys64/ucrt64/bin` on `PATH` (otherwise they exit silently, missing DLLs).

## Code layout
- `src/main.cpp`: capture, MOG2 loop, display, key controls.
- `src/args.hpp` / `src/args.cpp`: hand-rolled CLI parsing (Julian chose this over CLI11/cxxopts/`cv::CommandLineParser`). `InputFlags` struct (`video_path`, `headless`, `scale`, default 1.0), `argParse` throws `std::runtime_error` on any bad input (one catch in `main` prints the error + `printUsage`, returns 1). `getNextArg` is file-private (anonymous namespace).
- `src/ccl.hpp` / `src/ccl.cpp`: BFS connected-components labeling. Public: `Blob {bounding_box, area, centroid}` and `label(mask, labels, blobs)` (binary `CV_8UC1` in, `CV_32SC1` labels out, 0 = background, blobs numbered from 1 in scan order, 4-connectivity). `floodFill` and the neighbor tables are file-private. `#ifndef NDEBUG` check at the end of `label()`: blob areas sum to `countNonZero(mask)`. `main` thresholds the MOG2 mask at 200 (drops shadows = 127) before calling `label`; `render` draws boxes with `area >= blobAreaThreshold` (display-only filter).
- Usage: `main.exe --video_path <file> [--headless] [--scale <s>]`, `0 < s <= 1`. Headless skips all display work and `waitKey` (no pause/quit; Ctrl+C only).
- Display at the default scale 1.0 is a 3840x1080 window (too wide); use `--scale 0.5` for interactive runs.

## Data
- Source: 6 Pexels clips downloaded by hand (Sep 30) into `source-videos/`. No trimming needed; all clips are 21–60 s.
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
| 3 | Oct 6–12 | Implement BFS connected-components labeling → bounding boxes per blob. BFS labeling + blob boxes drawn + area-sum debug check ✅ (Oct 7, `src/ccl.*`, ~6 ms/frame at 540p). Left: compare vs. `cv::connectedComponentsWithStats`, row-pointer timing, 4 vs. 8 connectivity, weekly report |
| 4 | Oct 13–19 | Implement heap-based NMS; tune noise filtering (min blob size, morphology) |
| 5 | Oct 20–26 | Naive centroid tracking + line-crossing counting → first end-to-end counts |
| 6 | Oct 27–Nov 2 | Manual ground-truth count on the same clip; measure accuracy; iterate on misses/double-counts/occlusion |
| 7 | Nov 3–9 | Buffer/iteration on accuracy. **Decision point:** go/no-go on stretch structures based on time left |
| 8 | Nov 10–16 | Stretch: union-find and/or KD-tree tracking (if pursuing) — else polish + more test clips |
| 9 | Nov 17–23 | Stretch: speed estimation / per-lane counts (if time allows) |
| 10 | Nov 24–30 | Thanksgiving week, light load: formal write-up, code cleanup, finalize benchmark numbers |
| — | Dec 1–2 | Final polish, submit |

## Open questions
- ~~OpenCV version/install method not yet decided~~ → resolved Sep 30: OpenCV 5.0.0 via MSYS2 UCRT64 pacman
- ~~Confirm `BackgroundSubtractorMOG2` location/API in OpenCV 5~~ → resolved Sep 30: still `cv::createBackgroundSubtractorMOG2` (and `KNN`) in the `video` module, declared in `opencv2/video/background_segm.hpp` (`/ucrt64/include/opencv5/...`). Link with `-lopencv_video`.
- Exact line-crossing / counting-zone geometry not yet designed (store it in `meta.json` → `annotations.count_line`)
- ~~MOG2 shadow detection~~ → resolved Oct 1: `detectShadows = true`; threshold to a separate binary mask (only 255 = foreground) before BFS
- Downscale factor: **optional** for the pipeline (headless 1080p = 13.2 ms/frame, fits the 40 ms budget); needed only for interactive display. Keep as one variable `s` (`INTER_AREA`, before MOG2); decide 1.0 vs 0.5 in Week 4 on mask/blob accuracy. Timing table in `log.md`
- Warm-up: how many frames before counting starts (phantom-car ghost from frame 1); measure with the frame counter
- Count-line coordinates: scale down once at startup vs. scale results up per frame (`meta.json` stays full-res)
- ~~Display flag~~ → done Oct 4 (`--headless`). Still open: headless read-only run for decode's share; MOG2 on grayscale (see `log.md` 10-1-26)
- Optional: pass `cv::CAP_FFMPEG` to `VideoCapture` to skip GStreamer (noisy warnings on bad paths; makes the timed decoder explicit)
- Step forward/back controls: forward is simple; back needs a circular buffer (MOG2 can't un-learn). Optional
