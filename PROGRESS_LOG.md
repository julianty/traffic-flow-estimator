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
