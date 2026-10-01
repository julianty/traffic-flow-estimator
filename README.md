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

Source footage is fixed-camera traffic video from [Pexels](https://www.pexels.com/), downloaded by hand. Each clip was checked for camera motion, since background subtraction needs a static camera. Per-clip metadata (source URL, resolution/fps, stability, role) lives in `source-videos/meta.json`: the main clip is `auckland-hwy`, the validation clip is `auckland-fwy`, and the backup is `urban-road`.

Clips are used under the [Pexels License](https://www.pexels.com/license/) (free to use and modify, no attribution required, no redistributing unaltered copies), so the raw video is not checked into this repo (see `.gitignore`).

## Benchmark

Pipeline vehicle counts will be validated against a manually-counted ground truth on the same footage.
