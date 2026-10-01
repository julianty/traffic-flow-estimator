# Worklog

Subset of total work, meant to leave a record

### 9-30-26:

Transfer from mingw64 - ucrt64
Installed OpenCV 5.0.0 via pacman (`mingw-w64-ucrt-x86_64-opencv`); pkg-config name is `opencv5` (not `opencv4`), headers in `/ucrt64/include/opencv5`
Gotcha: hello.exe exited silently with code 127 (DLL not found). `ntldd -R` showed missing Qt6 DLLs (Qt6Core, Qt6Gui, Qt6Widgets, ...)

- HighGUI (`imshow`) in MSYS2's OpenCV 5 is built on Qt6, so Qt is required at runtime even though pacman lists it as optional
- Fix: `pacman -S mingw-w64-ucrt-x86_64-qt6-5compat` (pulls in qt6-base)
- Debugging note: `ldd`/`ntldd` report `api-ms-*` / `ext-ms-*` DLLs as "not found" -- these are Windows API-set aliases, not real missing files
  hello_opencv test program builds and runs from the UCRT64 terminal
  Downloaded 5 videos to use as source videos in source-videos/ from pexels for their free licenses

Footage (Week 1 done)
- Switched from YouTube/yt-dlp to Pexels (clearer license, no download tooling needed)
- Per-clip metadata lives in `source-videos/meta.json` (not here)
- Checked each clip for camera motion (feature matching vs. first frame every 2 s); background subtraction needs a static camera
  - Main: `auckland-hwy` (stable, dense). Backup: `urban-road` (stable, sparse, 4K60)
  - Added 6th clip `auckland-fwy` as validation: same creator as main (also Auckland), different framing, 90 s, stable
  - Drone clip `birdseye-4way` has the best angle but drifts ~50 px / rotates 1.6°, so it's unused for now
- Gotcha: file names aren't reliable. `6595761-uhd_3840_2160...` is actually 2560x1440; use ffprobe
- Pexels License: free to use/modify, no attribution needed, no redistributing unaltered copies, so raw videos stay gitignored

MOG2 in OpenCV 5: `createBackgroundSubtractorMOG2` still declared in `opencv2/video/background_segm.hpp` (found via grep in /ucrt64/include/opencv5); needs `-lopencv_video`

### 9-30-26 (Week 2: build + video I/O):

Build setup
- Makefile at repo root: one rule per exe, compile+link in a single g++ command, output in build/ (`@mkdir -p build` in the recipe). First rule (`build/main.exe`) is the default target. No .o files / header tracking yet; add a pattern rule + `-MMD -MP` when the second .cpp (BFS, Week 3) or first header arrives
- Ctrl+Shift+B runs `make` via `.vscode/tasks.json` (gitignored): type `shell`, cwd `${workspaceFolder}`, `options.shell` = MSYS2 `bash.exe` with args `--login -c`, `options.env` = `MSYSTEM=UCRT64`, `CHERE_INVOKING=1` (env values must be strings). Env test printed `UCRT64`, `/usr/bin/make`, `/ucrt64/bin/pkg-config`
- pkg-config: `--cflags` prints the `-I` (headers, compile step); `--libs` prints `-l` flags for ALL ~60 opencv5 modules (works, slow to link, hides real deps). `hello_opencv` still uses it; `main` lists libs explicitly
- Gotcha: `undefined reference to cv::Mat::Mat()` / `cv::imshow` / `cv::waitKey` = header found, library not linked. Mat is in core, imshow/waitKey in highgui, VideoCapture in videoio, MOG2 in video (needs `-lopencv_video`)

src/main.cpp so far
- Takes the video path as argv[1]; usage message + return 1 if missing; `isOpened()` check with error + return 1
- Bad path prints GStreamer warnings (OpenCV tries the string as a pipeline on the GStreamer backend); harmless, real files don't trigger it
- Properties match meta.json for auckland-hwy: 1920x1080, 25 fps, 1500 frames, 60 s
- Playback loop: `read` -> `imshow` -> `waitKey`; loop ends when `read` returns false; Esc exits cleanly (q handled in code, not yet tested)

Timing (auckland-hwy, 1080p, full clip, `waitKey(1)`)
- 28.8 s for 1500 frames = 52 fps, ~19 ms/frame (decode + imshow + waitKey), ~2.1x real time
- Real-time budget at 25 fps is 40 ms/frame, so ~20 ms/frame of headroom for MOG2 + BFS + NMS at full res (downscaling will add more)
- Caveat: that run used the file's frame count as the numerator, so it is only valid for a full run; use a loop counter for Esc-interrupted runs
- Gotchas: `time_since_epoch()` is not elapsed time (need a second `now()` and subtract); a `chrono::duration` does not convert to double, use `.count()`; `<<` on a duration is C++20

Open
- Qt prints `QThreadStorage: entry N destroyed before end of thread ...` at exit (HighGUI is Qt6-based). Likely shutdown-order noise; still to check whether `cv::destroyAllWindows()` after the loop silences it
- Next: MOG2 (add `-lopencv_video`) with mask shown next to the frame (hconcat + cvtColor GRAY2BGR), then shadows / warm-up / downscale decisions

### 10-1-26 (Week 2: MOG2 + mask display + downscale):

MOG2 running end-to-end on `auckland-hwy`, mask shown next to the frame (Week 2 goal met). Screenshots in `assets/`.

MOG2 API (OpenCV 5)
- `cv::createBackgroundSubtractorMOG2(int history=500, double varThreshold=16, bool detectShadows=true)` is a factory function, not a constructor. Returns `cv::Ptr<BackgroundSubtractorMOG2>` (smart pointer, shared_ptr-style refcount)
- `BackgroundSubtractorMOG2` -> `BackgroundSubtractor` -> `Algorithm`. Methods are pure virtual (`= 0`); the real implementation is a hidden class inside the library, reached through the vtable. That's why you get a pointer instead of constructing one
- Params: `history` sets learning rate ~1/history (exponential forgetting, not a 500-frame window; 500 @ 25 fps ~ 20 s, so a car stopped longer than that fades into background). `varThreshold` is squared Mahalanobis distance (16 ~ 4 sigma; higher = stricter, less noise, may lose faint cars). `detectShadows` marks shadows as 127 (fg 255, bg 0)
- MOG2 is stateful: create once before the loop, `apply` every frame (including warm-up). Recreating it per frame = model never learns
- Current settings: defaults, `detectShadows = true`. Plan: keep shadows in the display mask, threshold to a separate binary mask (only 255 = fg) before BFS. Caveat: dark shadows can still come out as 255

Gotchas
- Silent crash after the property printout, no error: `cv::BackgroundSubtractorMOG2* mog2 = cv::createBackgroundSubtractorMOG2();` compiles (`Ptr` converts implicitly to raw `T*`), but the returned temporary `Ptr` dies at the `;` -> refcount 0 -> object deleted -> `mog2` dangling -> access violation on first `apply`. Fix: store the `Ptr` itself (`cv::Ptr<...>` or `auto`). Rule: don't turn an owning smart pointer into a raw pointer unless something else keeps owning it
- Debugging: check `$LASTEXITCODE` in PowerShell after a silent exit (-1073741819 = 0xC0000005 access violation)
- `undefined reference` = header found, library not linked (compiler needs the declaration, linker needs the definition). `cvtColor` and `resize` are in `imgproc` (not `core`); `hconcat` is in `core`. Header path maps to the library: `opencv2/imgproc.hpp` -> `-lopencv_imgproc`. Makefile now links core, highgui, videoio, video, imgproc
- `cv::Mat` is a header + refcounted pixel buffer: `a = b` is shallow (shared pixels), `clone()`/`copyTo()` are deep. `cvtColor` into a separate empty Mat allocates a new buffer, so the 1-channel `mask` stays untouched for BFS
- `hconcat` needs matching rows and type: frame is 3 ch (BGR), mask is 1 ch -> convert mask to 3 ch into a display-only Mat first
- `&&` short-circuits: `!quit && cap.read(img)` avoids decoding one extra frame after quitting (`cap.read(img) && !quit` reads first)

Downscale
- `cv::resize(img, imgScaled, cv::Size(), 0.5, 0.5, cv::INTER_AREA)` right after `cap.read`, before `apply`. INTER_AREA is the recommended interpolation for shrinking; area averaging also smooths sensor noise a bit
- Everything after the resize uses the small frame (MOG2 + display), so `hconcat` sizes match and the window fits one monitor
- Coordinates: `meta.json` annotations stay in full-res pixels; scale by s = 0.5 when used (still to decide: scale the count line down once at startup vs. scale results up every frame)

Timing (auckland-hwy, full clip, 1500 frames, `waitKey(1)`, single runs)

| Resolution | Pipeline | Display | Run time | ms/frame | fps |
|---|---|---|---|---|---|
| 1920x1080 | decode only | frame | 28.8 s | 19.2 | 52.1 |
| 1920x1080 | decode + MOG2 | frame | 57.4 s | 38.3 | 26.1 |
| 1920x1080 | decode + MOG2 | frame + mask | 61.0 s | 40.6 | 24.6 |
| 960x540 | decode + resize + MOG2 | frame + mask | 33.4 s | 22.2 | 45.0 |
| 1920x1080 | decode + MOG2 (+ cvtColor) | none (headless) | 19.8 s | 13.2 | 75.9 |
| 960x540 | decode + resize + MOG2 (+ cvtColor) | none (headless) | 6.7 s | 4.5 | 222.3 |

Headless = `hconcat`/`putText`/`imshow` commented out; `waitKey(1)` still called but returns immediately with no window. Both headless runs had `detectShadows = true`. (A headless 540p run on `motorway-vertical` by mistake: 2.9 s / 540 frames = 5.3 ms/frame; different clip, lower bitrate, not comparable.)

- With display: 1080p + mask display was over the 40 ms budget (40.6 ms); downscale brought it to 22.2 ms (1.8x)
- **Correction (headless runs):** display is the big cost, not decode or MOG2. At 540p, display was ~18 of 22 ms (~80%). The fixed cost that didn't shrink with the downscale was mostly display, not decode (earlier "Amdahl + decode" explanation was wrong)
- The "MOG2 @ 1080p ~ 19 ms" difference was inflated by running alongside the window: the whole headless 1080p loop (decode + MOG2) is 13.2 ms
- Headless downscale speedup 13.2 -> 4.5 ms = 2.9x, close to the 4x pixel reduction, so most of the loop scales per pixel and decode is a small share
- **Downscale is optional for the pipeline:** headless full res leaves ~27 ms of the 40 ms budget for BFS/NMS/tracking. It's needed only for interactive display at 1080p. Keep the scale factor as one variable `s`; decide 1.0 vs 0.5 in Week 4 on mask/blob accuracy (small/distant cars), use 0.5 for interactive debugging
- Caveat: the 19.2 ms baseline is decode + imshow + waitKey, NOT decode alone; read-only headless run still needed for decode's true share
- Slack update draft blamed decode/MOG2 for the downscale: revise before posting (visualization was ~80% of the cost)
- Caveat: unsure whether the 960x540 run had `detectShadows` on or off; note the setting on future runs
- Paced run (`waitKey(1000/fps)` ~ 40 ms) took 127.5 s / 85 ms/frame: measures playback, not throughput. Benchmark with `waitKey(1)`
- Effective fps now uses the loop's `frame_count`, so early-quit runs report correctly

Warm-up
- Mask looked good almost immediately (MOG2's learning rate is high in the first frames, then settles toward 1/history)
- "Phantom car" seen in the mask early on: cars present in frame 1 become part of the initial background and leave a ghost when they drive off
- Plan: skip counting (not `apply`) for the first N frames; N still to measure with the frame counter

Playback controls (main.cpp)
- Space pauses: inner loop around `waitKey(0)` (blocks until a key). Space resumes, q/Q/Esc quits, other keys print a hint and keep waiting
- `break` only exits the innermost loop, so quitting while paused sets a `quit` flag checked in the outer `while` condition
- Nothing in the pause loop calls `read`/`apply`, so MOG2 isn't fed the frozen frame
- `waitKey` also runs the GUI event loop (imshow only paints during waitKey); window needs focus for keys
- Frame counter drawn with `putText` on the display image only (never on `imgScaled`, or MOG2 would see the text). Uses `frame_count` (1-based). Position is hardcoded (1730, 500); breaks if the scale changes

Display cost and decode (insights for the write-up)
- Display (`cvtColor`, `hconcat`, `putText`, `imshow`, waitKey GUI work) is overhead, not pipeline. Make display optional (flag): display mode for debugging/controls, headless for benchmarks and final counts. Report both numbers
- Decode is unavoidable for .mp4 (H.264 -> pixels), but turned out cheap (whole headless 1080p loop is 13 ms; FFmpeg decodes multithreaded), so these options are low priority. Options: pre-scale files offline with ffmpeg (decoder does ~1/4 the work, removes resize step; keep scaled files gitignored), raw/y4m (no decode but ~4.7 GB/min at 1080p), hardware decode (`CAP_PROP_HW_ACCELERATION`, build-dependent), decode thread + bounded producer-consumer queue (overlaps work; Week 8+ idea at most). H.264 is already a cheap codec
- Real deployments: IP cameras stream H.264/H.265 over RTSP but usually offer a low-res analytics substream (camera does the downscale); edge devices can run on pre-encode YUV frames where Y is grayscale for free; 10-15 fps often enough for line counting (too low and fast cars skip the line between frames). So these 1080p-file timings are conservative

Open
- Test cases for playback controls not run yet: play to end; q / Q / Esc while playing; space -> space (resumes same frame, cars didn't melt into bg); space -> q / Esc; space -> other key -> space; repeated pause/resume; quit halfway (fps should still be ~45); no argument; bad path
- Measure warm-up N (frame where the phantom car leaves the mask)
- Step forward (right arrow) while paused: one normal iteration, stay paused. Arrow keys need `waitKeyEx`; print the codes, Qt backend may differ from the Win32 values online. Pull per-frame work into a function
- Step back (left arrow): MOG2 can't un-learn, so seeking back gives a different mask. Options: re-run from frame 0 (slow) or circular buffer of the last N frames+masks (~2 MB/frame at 540p; must `clone()` into the buffer or every entry aliases the reused Mats). Optional
- Timing gaps: headless read-only (`cap.read` only) for decode's share; repeat key runs 2-3x; MOG2 on grayscale input
- Make display optional via a flag instead of commenting code out; make the scale factor a variable (also fixes the hardcoded putText position)
- Qt `QThreadStorage` message at exit: still to check
