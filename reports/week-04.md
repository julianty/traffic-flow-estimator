# Week 4 progress report (Oct 13 – Oct 19, 2026)

**Status: on track for the committed deliverable, with one design question still open.** Week 4's goal was heap-based non-max suppression (NMS) and noise tuning. The heap and the NMS function are written, tested and wired into the pipeline, and the mask is cleaner than last week. The main finding is that plain IoU does not suppress boxes nested inside a larger box, so NMS alone doesn't remove the fragments I hoped it would. Update this before sending on Sunday.

## Done this week

- **Heap-based NMS (my own code):** `BlobMaxHeap` is a max-heap of blobs ranked by area (ties go to the smaller `y`, then the smaller `x`), built in O(n) with `siftDown`. The `nms()` function pops the largest blob, drops it if its IoU with any kept box is over the threshold (strictly greater), and otherwise keeps it. It is wired into `main` after labeling, and the boxes drawn on screen are the kept ones.
- **Minimum-blob-size filter:** blobs under 100 px² (at half resolution) are removed with erase-remove before NMS. This was a Week 3 carry-over, and it replaces the display-only filter that used to live in `render`.
- **Blob count on screen:** a second line under the frame counter shows how many blobs were kept (an optional parameter of `render`).
- **MOG2 settings are named constants** (`history`, `varThreshold`, learning rate) and the learning rate is now fixed at 0.003 instead of MOG2's automatic rate (see Key findings).
- **Tests (written with Claude's help):** `scratch/nms_check.cpp` checks `nms()` on hand-made cases (empty input, identical boxes, disjoint and edge-touching boxes, a chain A–B–C where B is dropped but C survives, the exact-threshold boundary, threshold 1.0, output ordering) and on 2,000 random trials against two properties of greedy NMS. All pass. The heap test from last week is unchanged (233,800 pops, 0 failures).
- **A tuning tool (written with Claude's help):** `scratch/mog2_tune.cpp` renders one frame's mask under several MOG2 or morphology settings side by side, with blob counts. It replays the clip from frame 1, so the model has its normal history. The comparison images are in `assets/` (`Week 4 - ...`).
- **Timing:** headless on `auckland-hwy` at 0.5, three runs took 17.1–17.2 s for 1,500 frames (about 11.4 ms/frame, 87 fps). Last week's baseline was 10.6 ms/frame, so NMS and the filter cost about 7%.

## Key findings

- **IoU can't suppress a small box inside a big one.** IoU is intersection over union, so a small box fully inside a large one scores about the small box's area divided by the large one's, well under any sensible threshold. On `auckland-hwy` frame 336 the boxes inside and overlapping vehicles survived NMS at a threshold of 0.5. The cause is that blobs are connected components: the fragments inside a car are separate pieces of the mask, not duplicate detections of the car, which is what IoU-based NMS was designed for. The area filter removes the small specks, but larger fragments remain.
- **MOG2: only the learning rate mattered.** On the main clip, varThreshold (16, 12, 10), a Gaussian pre-blur (3, 5) and a longer `history` changed little, and the blur made things slightly worse. A fixed rate of 0.003 from the first frame cut the fragments sharply (frame 336: 21 → 13 blobs ≥100 px², 68 → 26 in total) because MOG2's automatic rate is very fast in the first ~250 frames and absorbs vehicles into the background in dense traffic. A fast warm-up followed by a slow rate (the obvious fix) gave the baseline result again, so the early phase is what hurts. The holes at windscreens and dark panels are not fixed by any setting.
- **The slow rate has a cost: ghosts at the start of the clip.** At frames 10–60 the fixed rate leaves smeared copies of the first frame's vehicles (170k foreground pixels at frame 10 vs. 10k for the automatic rate, which has already absorbed everything). They are gone by frame 100 on the main clip. I'll skip about 100 frames at the start when counting (a tentative number, not yet measured).
- **The validation clip didn't confirm the gain clearly.** On `auckland-fwy` the counts at 0.003 were about equal to the baseline (frame 336: 9 vs. 13 blobs ≥100 px²; frame 700: 8 vs. 7), and the masks looked similar. I kept 0.003 because it is no worse there and much better on the main clip.
- **Shadows are discarded.** Over 400 frames, 33% of the pixels MOG2 flagged were shadow (value 127); the threshold at 200 removed all of them and kept all the foreground.
- **The 7x7 close is the right size.** Counts and images at close sizes none, 3, 5, 7 and 9 on both clips: smaller kernels leave each vehicle in several pieces (152 blobs with no close at hwy frame 336), and 9 starts merging neighbouring vehicles (the dump truck fuses with the next vehicle). 7 gives mostly one box per vehicle.
- **4 vs. 8 connectivity makes no difference at the 7x7 close.** Counts were identical on all four frames tested. Without the close, 8 joins a few diagonal fragments (for example 28 → 24 on `auckland-fwy` frame 336). I'm keeping 4, the default.
- **Overlapping vehicles are a limit I can't fix with a kernel.** On `auckland-fwy` frame 336 the cement truck and the van in front of it touch in the mask, so they come out as one box (or two heavily overlapping boxes with no close at all). No close size separates them without also breaking up the other vehicles.

All of the tuning is judged by eye from two to four frames per clip, with blob counts as a proxy and no ground-truth masks, so the settings are my best choice for now. The Week 6 benchmark against a manual count is where they get tested properly.

## Still open

- **The NMS metric.** Keep plain IoU (the standard) and rely on the area filter, or switch to intersection over the smaller box's area, which suppresses nested boxes but is a non-standard variant. If I switch, `nms_check.cpp` needs updating (several expected values assume IoU).
- **The warm-up length** (about 100 frames is a guess), and whether to decide it before the manual ground-truth count in Week 6 so I don't have to recount.
- Merged vehicles (the truck and van case) are a known limitation and will show up as undercounts.
- Confirm the Week 3 report was sent.

## Next week (Week 5, Oct 20–26)

- Decide the NMS metric and the warm-up length.
- Naive centroid tracking and line-crossing counting, aiming for the first end-to-end counts. Blobs carry the centroid and box I need, and the tracker will assign its own IDs (the labeling's numbers aren't stable across frames). Tracks need to tolerate a few missed frames, since blobs near the area threshold flicker.

## Risks / open questions

- Fragments and merged vehicles both feed errors into the tracker; I'm not yet sure counts will be accurate enough without the stretch work.
- Settings were tuned by eye on a handful of frames; they may not carry over to other footage.
