# Traffic Flow Estimator

Estimating traffic flow from single fixed-camera video using classical (non-deep-learning) computer vision — background subtraction, connected-components blob detection, and centroid tracking, with the core data structures implemented from scratch.

Built as an Honors Contract project for CISC187 (Data Structures, C++).

## Why classical CV, not a detector?

The point of the project is the data structures, not the detections. Rather than dropping in an off-the-shelf object detector, vehicles are found and tracked with hand-implemented algorithms built on top of OpenCV's lower-level primitives (background subtraction, video I/O, basic image ops).

## Approach

1. **Background subtraction** (OpenCV `BackgroundSubtractorMOG2`/`KNN`) to get a foreground mask per frame.
2. **BFS-based connected-components labeling** (implemented from scratch) to extract vehicle blobs from the mask, cross-checked against `cv::findContours`.
3. **Heap-based non-max suppression** (implemented from scratch) to dedupe overlapping blob detections.
4. Centroid tracking and line-crossing counting to produce vehicle counts through a defined region of the frame.

### Stretch goals
- Union-find for merging track associations across frames
- KD-tree for nearest-neighbor frame-to-frame vehicle matching
- Per-vehicle speed estimation (pixel↔world calibration)
- Per-lane counts

## Status

Early setup — see `CLAUDE.md` for the full project scope, roadmap, and weekly plan.

## Tech stack

- C++17, g++
- OpenCV (video I/O, background subtraction, basic preprocessing)

## Data

Source footage is fixed-angle YouTube traffic-camera video, pulled with `yt-dlp`. Raw video is not checked into this repo (see `.gitignore`).

## Benchmark

Pipeline vehicle counts will be validated against a manually-counted ground truth on the same footage.
