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

## Tech stack
- C++17, g++, VS Code (current setup — works fine, no need to change)
- OpenCV 5.0.0 — installed via MSYS2 UCRT64 (`pacman -S mingw-w64-ucrt-x86_64-opencv`), verified Sep 30 with `scratch/hello_opencv.cpp`
  - pkg-config name is `opencv5`; headers in `C:/msys64/ucrt64/include/opencv5`
  - HighGUI (`imshow`) needs Qt6 at runtime: `mingw-w64-ucrt-x86_64-qt6-5compat` (pacman lists it as optional, but it isn't for us)
  - `C:\msys64\ucrt64\bin` is on Windows PATH so exes run outside the UCRT64 terminal
- ffmpeg — installed (came in as an OpenCV dependency)
- Build: VS Code default build task (`.vscode/tasks.json`, gitignored) with `-std=c++17 -IC:/msys64/ucrt64/include/opencv5 -lopencv_core -lopencv_imgproc -lopencv_highgui`; or `g++ ... $(pkg-config --cflags --libs opencv5)` in the UCRT64 terminal. Add `-lopencv_videoio -lopencv_video` for video/MOG2. Since `.vscode/` isn't versioned, a simple Makefile is worth adding — not urgent.

## Data
- Source: 6 Pexels clips downloaded by hand (Sep 30) into `source-videos/`. No trimming needed; all clips are 21–60 s.
- **`source-videos/meta.json` is the source of truth** for per-clip metadata (URL, resolution/fps, camera stability, role, and placeholders for count line, mask regions, ground truth). Refer to clips by their alias there.
  - **Main:** `auckland-hwy` (16516296): 1080p25, 60 s, tripod-stable, dense traffic with some overlap
  - **Validation:** `auckland-fwy` (16516297): 1080p25, 90 s, tripod-stable, same creator as main (also Auckland), different framing. Tune on main, then check here without re-tuning.
  - **Backup:** `urban-road` (14350696): 4K60, 37 s, tripod-stable, sparse traffic (easy case). Downscale before processing.
  - Unused: `birdseye-4way` (drone drift/rotation), `motorway-vertical` (portrait, mild drift). Excluded: `skyline-hwy` (moving camera).
- License: Pexels License. Free to use and modify, no attribution required, but no redistributing unaltered copies. Raw `.mp4`s stay out of git (`*.mp4` in `.gitignore`); `meta.json` is tracked.

## Reporting
- Weekly progress report (informal, self + advisor updates) — every Sunday.
- Formal write-up at the end, for Julian's own record.
- Candidate automation: Claude Code Desktop's **local scheduled tasks** (Routines page, or `/schedule` in a session) — e.g. a weekly prompt that reads the week's commits/diffs and drafts a progress-report entry. Requires the Desktop app open and the machine awake to fire. If you want it to run even when your machine is off, use a **cloud routine** instead of a local task.

## Roadmap (Sept 24 → Dec 2)

| Week | Dates | Focus |
|---|---|---|
| 1 | Sep 22–28 | Env setup: install/verify OpenCV, confirm g++ links it ✅ (Sep 30); pull 1–2 short clips ✅ (Sep 30, Pexels); git repo + this file ✅ |
| 2 | Sep 29–Oct 5 | Background subtraction running end-to-end on a sample clip; visualize foreground masks |
| 3 | Oct 6–12 | Implement BFS connected-components labeling → bounding boxes per blob |
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
- Exact line-crossing / counting-zone geometry not yet designed
