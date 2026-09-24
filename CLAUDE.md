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
- OpenCV — not yet installed/verified
- Build: ad hoc `g++` compile for now. Once OpenCV flags get unwieldy, a simple Makefile is worth adding — not urgent.

## Data
- Source: YouTube traffic-camera footage, single fixed angle. No footage downloaded yet.
- Note: keep raw video out of git (`.gitignore`); check licensing before making the repo public with embedded footage. `yt-dlp` for pulling clips.

## Reporting
- Weekly progress report (informal, self + advisor updates) — every Sunday.
- Formal write-up at the end, for Julian's own record.
- Candidate automation: Claude Code Desktop's **local scheduled tasks** (Routines page, or `/schedule` in a session) — e.g. a weekly prompt that reads the week's commits/diffs and drafts a progress-report entry. Requires the Desktop app open and the machine awake to fire. If you want it to run even when your machine is off, use a **cloud routine** instead of a local task.

## Roadmap (Sept 24 → Dec 2)

| Week | Dates | Focus |
|---|---|---|
| 1 | Sep 22–28 | Env setup: install/verify OpenCV, confirm g++ links it; pull 1–2 short YouTube clips; git repo + this file |
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
- OpenCV version/install method not yet decided
- Exact line-crossing / counting-zone geometry not yet designed
