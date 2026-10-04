# Week 2 progress report (Sep 29 – Oct 4, 2026)

**Status: on track.** Week 2's goal was background subtraction (MOG2) running end-to-end with the mask displayed. That's done, plus timing measurements and a cleanup so the program has a fast benchmark mode. Week 3 (BFS connected components) starts Monday Oct 6.

## Done this week

- **Build system:** a Makefile builds the program with g++ and the OpenCV libraries it needs. Ctrl+Shift+B builds through the MSYS2 environment. The build now handles more than one source file (`src/args.cpp`), which BFS will need next week.
- **Video playback:** the program reads the main clip (`auckland-hwy`, 1080p, 25 fps, 60 s) frame by frame. Properties match the clip's recorded metadata.
- **Background subtraction:** OpenCV's MOG2 runs on every frame, and its foreground mask is shown next to the frame. Shadows are detected and marked separately (gray, not white), so they can be dropped before blob detection.
- **Playback controls:** space pauses and resumes; q or Esc quits; a frame counter is drawn on screen. Tested and working.
- **Command-line options:** `--video_path <file>`, `--headless` (no window, used for benchmarks), and `--scale <s>` (resize frames before processing). Any bad input gives a clear error message plus usage.

## Key findings

Headless timings on the main clip (1500 frames, one run each):

| Resolution | Headless ms/frame | With display ms/frame |
|---|---|---|
| 1920×1080 (scale 1.0) | 13.1 | 40.6 |
| 960×540 (scale 0.5) | 4.5 | 22.2 |

- **Real time at 25 fps means 40 ms per frame.** The full-resolution pipeline (decoding + MOG2) uses 13 ms. That leaves about 27 ms for BFS, NMS, and tracking without downscaling.
- **Most of the cost was the display, not the processing.** At 540p, showing the window took about 18 of 22 ms (~80%). I had first assumed decoding or MOG2 was the bottleneck; the headless runs showed that was wrong.
- **So downscaling is optional for the pipeline.** It's needed only so the debug window fits the screen. Whether to process at full or half resolution will be decided in Week 4, based on whether small or distant cars survive in the mask.
- **Warm-up effect:** cars already in the first frame become part of the learned background and leave a "ghost" when they drive away. Counting will need to skip the first N frames. N isn't measured yet.

## Next week (Week 3, Oct 6–12)

- Implement **BFS connected-components labeling** myself: turn the foreground mask into one bounding box (plus area and centroid) per blob.
- Check it against OpenCV's own connected-components function on the same masks. Blob counts and boxes should match exactly.
- Leftovers from Week 2: measure the warm-up frame count, place the frame counter correctly at any resolution, and split the main loop into process / display / input functions.

## Risks / open questions

- **Dense traffic on the main clip:** overlapping cars will likely merge into one blob. This is expected and will show up as undercounts. Weeks 6–7 are reserved for accuracy work.
- **Counting-line placement** isn't designed yet. It's needed by Week 5.
