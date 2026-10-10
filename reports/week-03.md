# Week 3 progress report (Oct 6 – Oct 12, 2026)

**Status: mostly on track, draft written Fri Oct 9, updated Sat Oct 10 after merging the Mac and Windows work.** Week 3's goal was BFS connected-components labeling that turns the foreground mask into one bounding box per blob. The labeling works, is in the pipeline, and matches OpenCV's own function exactly; some cleanup is still open. Update this before sending on Sunday.

## Done this week

- **BFS connected-components labeling (my own implementation):** a flood fill over the foreground mask, 4-connected, returns a bounding box, area and centroid for each blob. Boxes are drawn on the frame.
- **4 vs. 8 connectivity flag:** `--kneighbors <4|8>` (default 4) chooses whether diagonal pixels join a blob. 4 is the default because it is what was checked against OpenCV and because 8 can only merge more cars. Not yet compared on blob counts.
- **Noise cleanup before labeling:** the mask now goes through a morphological open (3x3, removes specks) then close (7x7, fills holes and gaps in cars) before labeling. The small noisy blobs are gone on the main clip. It costs almost nothing: 12.8 ms/frame at half resolution, unchanged from before.
- **New test footage:** four I-5 overpass clips. The raw clips drifted by 59–107 px, too much for background subtraction, so I stabilized them in software. `i5-incoming` came out at 1.3 px and is now the development clip for labeling (sparse traffic, clean masks). `i5-outgoing` is marginal (7.7 px) and unused for now.
- **Housekeeping:** clip metadata updated for the new footage; macOS editor setup fixed.
- **Merged the two machines' work (Oct 10):** the Windows work (labeling split into `ccl.cpp`/`ccl.hpp`) and the Mac work (portable build, morphology, the OpenCV comparison test) had been developed on separate branches. They are now merged into `main` and pushed. Three files conflicted (the Makefile, `main.cpp`, the log) and were resolved by hand. After the merge the project builds cleanly on Windows and processes the full main clip (1,500 frames) at half resolution in 15.9 s, about 10.6 ms/frame.

## Key findings

- **My labeling matches OpenCV exactly.** A test program runs my BFS and OpenCV's `connectedComponentsWithStats` on the same mask every frame and compares blob count, bounding box, area and centroid. Zero mismatches over 3,755 frames on both Auckland clips (half and full resolution). Not yet covered: blobs touching the frame edge, and the i5 clip.
- Morphology order matters: open first, then close. They are not inverses of each other, since each loses information that the other can't restore.
- A car's shadow on the shoulder joins its blob in the i5 clip, which will skew boxes and centroids. This needs tuning in Week 4.
- Full-resolution processing runs at about 46 ms/frame on the Mac, over the 40 ms real-time budget at 25 fps. Half resolution is 12.8 ms. This feeds the scale decision in Week 4.

## Still open from Week 3 (to finish or carry over)

- Run the OpenCV comparison at 8-connectivity and compare 4 vs. 8 blob counts on the dense clip.
- **Merged cars are a confirmed problem** on the dense Auckland clip (noted Oct 10). Non-max suppression won't split one merged blob, so Week 4 needs a smaller close kernel and/or a split step as well.
- Point the comparison test at the shared `ccl.hpp` instead of its own copy of the labeling code, so the two can't drift apart. (The code split itself is done and merged.)
- The minimum-blob-size filter was removed from the drawing code but not re-added anywhere, so no stage filters tiny blobs yet.
- Check that the close step doesn't merge two nearby cars into one box.

## Next week (Week 4, Oct 13–19)

- Implement **heap-based non-max suppression** to dedupe overlapping detections.
- Tune noise filtering (minimum blob size, kernel sizes, shadow handling) and decide full vs. half resolution.

## Risks / open questions

- Dense traffic on the main clip will merge overlapping cars; expected to show as undercounts.
- The i5 clips are sparse, so they test labeling but not overlap handling; the main clip has to carry that.
