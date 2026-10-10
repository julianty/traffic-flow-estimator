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

| Resolution | Pipeline                            | Display         | Run time | ms/frame | fps   |
| ---------- | ----------------------------------- | --------------- | -------- | -------- | ----- |
| 1920x1080  | decode only                         | frame           | 28.8 s   | 19.2     | 52.1  |
| 1920x1080  | decode + MOG2                       | frame           | 57.4 s   | 38.3     | 26.1  |
| 1920x1080  | decode + MOG2                       | frame + mask    | 61.0 s   | 40.6     | 24.6  |
| 960x540    | decode + resize + MOG2              | frame + mask    | 33.4 s   | 22.2     | 45.0  |
| 1920x1080  | decode + MOG2 (+ cvtColor)          | none (headless) | 19.8 s   | 13.2     | 75.9  |
| 960x540    | decode + resize + MOG2 (+ cvtColor) | none (headless) | 6.7 s    | 4.5      | 222.3 |

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
- ~~Make display optional via a flag; make the scale factor a variable~~ -> done 10-4 (`--headless`, `--scale`)
- ~~Qt `QThreadStorage` message at exit~~ -> checked 10-4: harmless, see below

### 10-3-26 / 10-4-26 (Week 2: refactor, CLI flags, headless mode):

CLI flags (hand-rolled, `src/args.hpp` / `src/args.cpp`)

- Usage: `main.exe --video_path <file> [--headless] [--scale <s>]`, `0 < s <= 1`, default scale 1.0. Replaces `argv[1]`
- Considered CLI11 / cxxopts (both in UCRT64 pacman) and `cv::CommandLineParser` (already linked via core); hand-rolled since there are only 3 options and one program
- Pattern: `InputFlags` struct with defaults in the member declarations; loop over `argv` from 1; flags that take a value use `getNextArg` (bounds check, then `++i` on the caller's index, so `i` is passed by reference); unknown args throw; required/range checks after the loop
- Errors: everything thrown out of `argParse` is a `std::runtime_error`; one catch in `main` prints the message + `printUsage(argv[0])`, returns 1
- `getNextArg` is file-private: anonymous namespace in `args.cpp` (internal linkage; never put one in a header). Leading-underscore names are reserved at global scope in C++ (Python habit doesn't carry over)
- Gotchas
  - `char*` == `char*` compares addresses; wrap `argv[i]` in `std::string` first
  - `std::stod` throws `invalid_argument` / `out_of_range`, which are `logic_error`, not `runtime_error`; a `catch (runtime_error&)` misses them -> `std::terminate`. Wrap: catch `std::exception` around only the `stod`, rethrow as `runtime_error` naming the flag and value
  - `stod("0.5abc")` returns 0.5 silently; check the `pos` out-param equals the string length
  - A catch catches your own throws too: the range check inside the `try` got rewrapped as "expects a number". Keep the try around only the call being translated
  - "Required" can only be checked after the loop (a check inside the `--video_path` branch never runs when the flag is missing)
  - Catch block must return; printing and continuing opened an empty path and buried the real error under GStreamer warnings
- Tested: no args, `--headless` only, `--video_path`/`--scale` with no value, `--scale abc`, `--scale 0.5abc`, `--scale 0/2/-1`, `--bogus`: each one clean line + usage, exit 1. Bad path still gets GStreamer warnings (parse OK, OpenCV rejects the file); passing `cv::CAP_FFMPEG` to `VideoCapture` would skip GStreamer

Build: first multi-file build

- Kept the Makefile simple: one rule, prerequisites list `main.cpp`, `args.cpp` **and** `args.hpp` (manual stand-in for `-MMD`, so a header edit rebuilds), `g++` line names both `.cpp` files (`$<` = first prereq only -> undefined reference; `$^` would pass the header to g++). Every change recompiles everything; fine at this size
- Header holds the struct + declarations only (`#pragma once`); bodies in the `.cpp`. Body in a header included twice = multiple definition; missing `.cpp` in the build = undefined reference

Headless mode (`--headless`)

- Display (`cvtColor`, `hconcat`, `putText`, `imshow`) and all key handling moved behind `!flags.headless`; headless skips `waitKey` too (no window, so no keys; Ctrl+C only). `mask3c` / `frame` moved out of the loop (no per-frame allocation)
- Removed the unused per-frame `frame_time_ms` (`cap.get(FPS)` every frame)
- Bug caught: a leftover local `double scale = 0.5;` overrode the flag, so `--scale` silently did nothing

Timing after refactor (auckland-hwy, 1500 frames, single runs)

| Scale | Mode     | Run time | ms/frame | Before (10-1) |
| ----- | -------- | -------- | -------- | ------------- |
| 1.0   | headless | 19.6 s   | 13.1     | 13.2          |
| 0.5   | headless | 6.8 s    | 4.5      | 4.5           |

- Pipeline unchanged by the refactor. Dropping the headless `cvtColor` saved <= 0.1 ms/frame: within noise, not a real speedup
- At 1.0, `resize` still runs (a full-frame copy for nothing); skip it when scale == 1.0
- Display run on `auckland-fwy` (validation clip, 2255 frames, 90 s) at 0.5: 36.1 s = 16.0 ms/frame (62.5 fps). Not comparable to the auckland-hwy 22.2 ms (different clip/content); playback only, no tuning on the validation clip
- Display at the default scale 1.0 is a 3840x1080 window (wider than the monitor); use `--scale 0.5` for interactive runs

Playback controls: pause/resume and quit re-tested after the refactor, working

Frame counter position

- Now `scale * (3460, 1000)`: correct for both scales on 1080p sources, but still tied to source resolution (a 4K source at 0.5 is 3840x1080 per pair and puts the text mid-frame). Next: derive from `frame.cols` / `frame.rows`, scale the font size too

Qt `QThreadStorage: entry N destroyed before end of thread` at exit: still printed with `cv::destroyAllWindows()` called, so that doesn't silence it. Shutdown-order noise from the Qt6 HighGUI backend; harmless, leaving it

Open

- ~~Refactor Step 2 leftovers~~ -> position done 10-4 (see below); font size and resize skip still open
- ~~Refactor Step 3~~ -> done 10-4 (see below)
- Measure warm-up N; headless read-only timing for decode's share; MOG2 on grayscale

### 10-4-26 (Week 2: refactor loop into functions):

Loop body is now `process` -> `frame_count++` -> (display only) `render` -> `handleInput`. All three are file-private (anonymous namespace in `main.cpp`). Parameter order rule for both Mat functions: inputs first, then outputs (OpenCV style)

- `process(img, mog2, scale, imgScaled, mask)`: resize + MOG2 `apply`. BFS goes here next
- `render(imgScaled, mask, frame_count, mask3c, frame)`: `cvtColor` (now `GRAY2BGR`, matching OpenCV's BGR convention; output identical for gray input), `hconcat`, `putText`, `imshow`. No `waitKey` (input's job; `imshow` paints during the next `waitKey`, so render must come before input)
- `handleInput()` returns `enum class KeyAction { Continue, Quit }`. Pause loop uses `return` instead of `break` + flag, so the `quit` flag is gone; main loop is `while (cap.read(img))` and `break`s on `Quit` (still no extra decode after quitting)
- Key codes: `constexpr int KEY_ESC = 27`, `KEY_SPACE = ' '`; `'q'`/`'Q'` as char literals (a char literal is its ASCII code). `isQuitKey(int)` dedupes the Esc/q/Q check. Plain constants, not an enum class, because `waitKey` returns `int` and an enum class won't compare to it. Numbers stay inside `handleInput`, only meanings come out
- Frame counter anchored to `(frame.cols - 220, frame.rows - 30)`: works for any source resolution and scale. Font size still fixed at 1.2
- Stopwatch: `auto start = std::chrono::steady_clock::now()` (static member, called on the type with `::`; no clock object). `dur` keeps the explicit `duration<double>`: that's what converts to seconds. With `auto` it would stay integer nanoseconds and the fps would be off by 1e9. Added `#include <chrono>` (was only arriving through OpenCV)

Gotchas

- Scope: a function sees only its params/locals/globals; `main`'s Mats must be passed in. Outputs by non-const `&` (a reference is the caller's object, and keeping the Mats in `main` lets `resize`/`apply` reuse their buffers every frame)
- `const cv::Mat&` as an OpenCV output **compiles** (`OutputArray` accepts it) but fails at runtime on the first frame: `(-215:Assertion failed) !fixedSize() || ...size == _sz in function 'create'`. `fixedSize()` in an assertion = a const Mat passed as an output
- `const cv::Ptr<T>&` still allows `mog2->apply` (const pointer, not const pointee; like `T* const` vs `const T*`)
- Non-void function with a path that reaches `}` without a `return` = undefined behavior (garbage return value, random quits), only a warning (`-Wall`). Hit by the no-key path (almost every frame) and by space-resume `break`
- Returning `Continue` for "other key while paused" silently resumed playback; that branch must only print the hint and keep looping
- `if (cond) return true; else return false;` -> just `return cond;`
- `.` is for objects, `::` for types/namespaces/static members: `steady_clock.now()` doesn't compile

Checks

- `-Wall -Wextra`: no warnings from our code. Two `multi-line comment` warnings from OpenCV's `photo/ccm.hpp`, pulled in by the `opencv2/opencv.hpp` umbrella header; replacing it with the specific headers silences them
- Headless after the refactor: 13.7 ms/frame @1.0, 4.4-4.7 ms @0.5 (single runs; vs 13.1 / 4.5 before, within run-to-run noise)
- Display-mode playback list not yet re-run after the `handleInput` change (esp. space -> other key -> stays paused)

Open

- ~~Font size scaled with frame height; skip `resize` when scale == 1.0~~ -> done 10-4 (see below)
- Add `-Wall -Wextra` to the Makefile + swap `opencv2/opencv.hpp` for specific headers
- Re-run the display-mode playback list
- Measure warm-up N; headless read-only timing for decode's share; MOG2 on grayscale

### 10-4-26 (Week 2: resize skip + scaled frame counter):

Skip `resize` at scale 1.0

- `process` does `imgScaled = img` (shallow copy: header + refcount, shared pixels, no copy) when `scale == 1.0`. Exact `==` on a double is safe here: 1.0 is exactly representable and comes straight from the default / `stod`, no arithmetic in between
- Aliasing rule: at 1.0, `imgScaled` _is_ `img`. Fine today (MOG2 only reads it, `hconcat` copies it, next `cap.read` comes after we're done). **Never draw on pipeline images (`img`, `imgScaled`, `mask`); draw only on the display image `frame`.** Drawing on `imgScaled` before `apply` would feed boxes/text to MOG2 at any scale (matters for Week 3 bounding boxes)
- Headless @1.0: 19.2 s / 18.9 s = 12.6-12.8 ms/frame (was 13.1-13.7). The resize at factor 1 cost ~0.5-1 ms/frame. @0.5 unchanged (4.4 ms)

Frame counter scaled with frame size

- Reference values tuned at 540p as `constexpr` (`refFrameHeight = 540`, `refFontScale = 1.2`, `refFontWt = 2`, `refMargin = 20`); `ratio = frame.rows / 540.0` scales font scale, thickness and margin
- Thickness is an `int` px: `std::max(1, lround(2 * ratio))`. Passing the `double` product truncated silently and could hit 0 (invalid) on small frames
- Position from `cv::getTextSize` (returns width/height; baseline via an out-param): `x = cols - textWidth - margin`, `y = rows - margin - baseline`. `putText`'s origin is the bottom-left of the **baseline**, not the top-left; descenders hang below by `baseline`
- Measures a fixed sample `"Frame: 0000"`, not the live string, so the right-aligned text doesn't slide left as digits are added (longest clip is 2255 frames)
- Font face is one named constant so measure and draw can't disagree
- Gotcha: `getTextSize` result was first discarded (call with no assignment), so the hardcoded 220 was still used
- IntelliSense flags `getTextSize` ("no instance of overloaded function matches"): false positive. OpenCV 5 added a second overload (`Rect getTextSize(Size imgsize, const String& text, Point org, ...)`, `imgproc.hpp` ~4402). g++ compiles cleanly with `-Wall -Wextra`
- Display checked at 0.5 and 1.0: counter bottom-right of the mask half, same relative size

### 10-7-26 (Week 3: new i5 clips + stabilization check):

Added 4 clips: `i5-{incoming,outgoing}-1080p-{30,60}fps-stab.mp4` (1080p, overpass view of I-5, 36-37 s; 30/60 fps are the same footage)

- Same stability test as 9-30 (ORB + RANSAC similarity fit vs. first frame, every 2 s, max corner shift in full-res px). Re-ran on `auckland-hwy` first as a sanity check: 1.8 px (meta.json says 1), so the method reproduces
- Raw clips are **not** usable: incoming 58.7 px, outgoing 107 px (60 fps versions same). Continuous wander (5-40 px between 2 s samples), not a one-time settle, so "stab" in the file name is not enough. MOG2 would flag lane lines/median as foreground and a fixed count line would not stay on the lanes
- Visually confirmed (median/lane lines in different places at start vs. end)

Software stabilization in DaVinci Resolve (Color page Tracker -> Stabilizer; masked to the static road/median, Camera Lock on, Zoom off; H.264 1080p30 export), re-measured

- `i5-incoming-1080p-30fps-softwarestab`: **1.3 px**, 0.03 deg, 1095 frames/36.5 s (same as source). Pass (target <= 2-3 px). Worst outer 8 px band 2.7% near-black (small crop gap: keep the count line away from the edges)
  - MOG2 (1080p -> 0.5 scale, defaults, shadows on, 255 only, open 3x3 + close 7x7, min area 150): mean fg 5.9%, 2.9 blobs/frame (max 8), one solid blob per car. Mask is clean on lane lines/median
  - Problem: a car's dark shadow on the shoulder joins its blob (wedge attached to the car), which will skew bounding boxes/centroids. Look at MOG2 shadow handling / morphology
  - Traffic is sparse (1-3 cars in view): easy for BFS but little overlap for NMS, and a small ground-truth count means one miss is a big % error
- `i5-outgoing-1080p-30fps-softwarestab`: **7.7 px**, creeping up from 2 px (inlier ratio only 4-14%, so low confidence; trend probably real). Marginal
  - Stabilizer left black bars at top/bottom that change size per frame (Zoom was off). Re-export with Zoom ~5-10% if this clip is wanted
  - MOG2 blobs fragmented/hollow (12.9 blobs/frame, max 24), shadows attached. Denser traffic with side-by-side cars: good stress case, but weaker than `auckland-hwy` (1.8 px, 60 s, stable)

Decisions

- Week 3 BFS development on `i5-incoming` (softwarestab): clean masks so any bug is in the labeling, not the input
- Main for benchmark / later stress (NMS, merging, Week 4+): stays `auckland-hwy`. `i5-outgoing` optional extra after re-stabilizing
- 60 fps incoming: skipped for now. Unstabilized (58.5 px), would need redoing in Resolve. MOG2 `history` is in frames (500 = ~17 s @30, ~8 s @60), so parameters don't transfer between frame rates. 60 fps leaves 16.7 ms/frame vs. ~13 ms headless at 1080p. Optional Week 8-9 experiment (same pipeline @30 vs. @60 with frame-rate-scaled params), or if speed estimation is attempted

Open

- Update `source-videos/meta.json` with the i5 clips (stability numbers above, roles: incoming = BFS dev, outgoing = unused/optional); `auckland-hwy` unchanged as main
- Tune MOG2 shadow threshold / morphology so car shadows don't join blobs
- Delete `scratch/_frames_compare.jpg` (leftover from the analysis)

### 10-7-26 (Week 3: BFS connected-components labeling, first working version):

`src/ccl.{hpp,cpp}`: `label(mask, labels, blobs)` + private `floodFill` (BFS, `std::queue<cv::Point>`), 4-connectivity, `Blob {bounding_box, area, centroid}`

- Binary threshold (`cv::threshold(mask, binMask, 200, 255, THRESH_BINARY)`) sits in `main` between `process` and `label`. Shadows (127) drop out; `render` still shows the raw `mask`
- `labels` is `CV_32SC1`, reused across frames (`create` + `setTo(0)` every call: `create` doesn't zero). 0 = background, blobs numbered from 1 in scan order. Separate Mat from the frame; `CV_32S` chosen over `CV_8U` (overflow past 255 labels on raw noise) and `CV_16U` (fine, but 32S matches OpenCV's `ltype`). Revisit 16U / no label image only if timing needs it
- Rule that fixed the main bug: **claim at enqueue, check before enqueue.** A neighbor is bounds-checked, tested (foreground and label == 0), labeled, then pushed. Nothing is re-tested at pop. Testing `labels == 0` on the popped pixel can never pass (it was labeled when pushed), so the BFS never expanded
- Stats (min/max x/y, count, sumX/sumY) are plain locals inside `floodFill`, the `Blob` is built once after the queue empties. Box width/height need `+1` (max is inclusive). `int minX, maxX = x;` only initializes `maxX` (the comma doesn't share the initializer): all boxes started at (0,0)
- Gotchas hit: `at<T>(row, col)` is `(y, x)` but `cv::Point` is `(x, y)` (swapped once on the seed write, an out-of-bounds write on non-square images; `at<T>(Point)` does the swap). `at<uchar>` on a `CV_32S` Mat reads the wrong bytes. `const cv::Point[]` not `constexpr` (`Point_`'s ctor isn't constexpr). `std::queue::pop()` returns void (`front()` then `pop()`). `{1 -1}` is `{0}`, a missing comma
- Debug check at the end of `label()` under `#ifndef NDEBUG`: sum of blob areas == `cv::countNonZero(mask)`. Passed on every frame of `auckland-hwy` (1500) and `auckland-fwy` (2255) at scale 0.5. Doesn't catch merged/split blobs (that's the OpenCV comparison)
- `render` draws blob boxes on `frame` (never on `imgScaled`), skipping `area < blobAreaThreshold` (100, display-only for now)
- Observed on frame 217 of an Auckland clip: boxes match the white blobs exactly, but cast shadows are solid 255 (not 127), so each box covers car + shadow. Same shadow problem as the i5 note above; centroids will be biased toward the shadow too

Timing (headless, `--scale 0.5`, `auckland-hwy`, `at<>` access, debug check on): 16.3 s / 1500 frames = ~10.9 ms/frame (was 4.5), so labeling is ~6 ms/frame at 540p, more than MOG2. Expect ~4x at 1080p

Open

- Compare against `cv::connectedComponentsWithStats` (connectivity 4): blob count, area, box per frame
- Time with row pointers (`ptr<uchar>`/`ptr<int>`) instead of `at<>`, and with `-DNDEBUG` (cost of the check)
- Try `setShadowThreshold` lower than 0.5 (darker pixels count as shadow) on frame 217; watch for holes in the black car. Then morphology (Week 4)
- Connectivity 4 vs. 8: `kNeighbors8` exists but `floodFill` is hard-wired to 4; make it a parameter, check on real masks
- `sumX`/`sumY` to `long long`; bounds check as `< 0 || >= cols`
- Week 4: min-size filter belongs in the pipeline, not just `render`
- Weekly report for Week 3 (Sunday Oct 11), `reports/week-03.md`

### 10-9-26 (Week 3: morphology before labeling, macOS):

Morphology on the binary mask (branch `morphology`, pushed; not merged)

- Pipeline now: `threshold` (>200, drops the gray shadow value) -> `morphologyEx` open 3x3 -> close 7x7 -> `label`. Kernels from `getStructuringElement(MORPH_RECT, ...)`, built once before the loop; sizes are named constants (`openKernelSize`, `closeKernelSize`)
- Kernel sizes are in pixels at the processing scale: 7x7 at `--scale 0.5` covers 14 px at 1080p. Changing the scale means retuning
- Open = erode then dilate (removes white specks narrower than the kernel, survivors regrow to ~original size). Close = dilate then erode (fills holes/gaps narrower than the kernel). Neither is the inverse of the other: the first step destroys information the second can't recover. Open first, otherwise close bridges specks into real blobs
- Morphology is the non-linear branch of spatial filtering (min/max over the kernel shape); convolution (blur, Sobel) is the linear branch (weighted sum)
- Gotcha: first version called `label(binMask, ...)`, so the cleaned `filterMask` was computed and discarded. Looked fine, boxes identical to before. Now `label(filterMask, ...)` and `render` shows `filterMask`
- Chaining the close in place on `filterMask` is fine; `dst` must be a non-const `Mat` (same trap as the `const Mat&` output in 10-4)
- Visual check on `auckland-hwy`: small noisy blobs gone
- Headless on `auckland-hwy`, 1500 frames, `-g` build: 19.1 s @0.5 (12.8 ms/frame, 78 fps), 68.8 s @1.0 (45.8 ms/frame, 21.8 fps). Matches the 10-9 numbers (13.6 / 47.7 ms), so morphology is nearly free. @1.0 is over the 40 ms real-time budget at 25 fps: BFS is the likely cost, revisit with the scale decision in Week 4

Setup

- macOS IntelliSense: red squiggles under `opencv2` were editor-only (`make` compiled fine). Fixed with a gitignored `.vscode/c_cpp_properties.json` (include `/opt/homebrew/opt/opencv/include/opencv5`, c++17, `macos-clang-arm64`)
- `source-videos/meta.json`: added `i5-incoming` (new role `dev`, 1.3 px) and `i5-outgoing` (unused, 7.7 px) from the 10-7 numbers. `source_url`/`author`/`codec`/`audio` left null: the i5 files are not on the Mac, run ffprobe on Windows to fill them
- `scratch/_frames_compare.jpg` doesn't exist on this machine; check on Windows

Open

- `ccl.hpp`/`ccl.cpp` split exists on the Windows PC only (not pushed). Merge `morphology` with it on Windows: expect small conflicts in `main.cpp` and the Makefile prerequisite/`g++` lists
- Blob area filter (100) is only applied in `render`, so later stages (NMS, tracking) would see tiny blobs. Move it into `label`/a cleanup step, and reconcile with the 150 used in the 10-7 analysis
- Check box merging on two close cars (does the 7x7 close join them?) on `auckland-hwy`
- `connectedComponentsWithStats` (4-connectivity) check against `label`: blob count, areas, boxes should match exactly, compared before the area filter
- `kNeighbors8` unused: delete it, or make connectivity a parameter and compare
- Local `main` is 3 commits ahead of `origin/main` (unpushed); `cross-platform-setup` branch still to delete after the Windows check

### 10-9-26 (Week 3: BFS vs. `connectedComponentsWithStats` check, macOS):

Harness: `scratch/ccl_check.cpp` (Makefile target `build/ccl_check.exe`, built by name, not by plain `make`)

- Copy of the pipeline up to `filterMask` (resize, MOG2, threshold, open 3x3, close 7x7), then runs my `label` and `cv::connectedComponentsWithStats(filterMask, ..., 4, CV_32S)` on the same mask each frame. `Blob`/`floodFill`/`label` are copied in, so they can drift from `main.cpp` (on Windows, point it at `ccl.hpp` instead)
- OpenCV output is converted into the same `Blob` type, both vectors are sorted with one comparator (top-left `y`, then `x`), then `compareBlobs` checks size, then box / area / centroid separately, printing frame + both values on any difference. Per-frame counters printed at the end
- Result: **0 mismatches** on `auckland-hwy` @0.5 (1500 frames) and `auckland-fwy` @1.0 (2255 frames). Boxes, areas and centroids identical on every blob
- Run time for the full check @1.0 on `auckland-fwy`: 115.7 s (19.5 fps). Includes decode, MOG2, both labelers and the compare, so it is not a BFS cost

What I learned

- `stats` is `CV_32S`, `numLabels x 5` (`CC_STAT_LEFT/TOP/WIDTH/HEIGHT/AREA`); `centroids` is `CV_64F`, `numLabels x 2`. **Row 0 is the background**, so blobs are rows 1..`numLabels-1`
- `CC_STAT_WIDTH/HEIGHT` are already pixel counts. My own `maxX - minX + 1` convention is inclusive-last-index; mixing the two (`minX + width` then `+ 1`) made every box one pixel too wide
- Label numbering/order is not guaranteed to match, so compare as sorted sets, not by index
- Centroids: mine is integer division (floor for non-negative), OpenCV's is double truncated by `cv::Point(double, double)`: also floor. They agree unless OpenCV's double lands just below an exact integer (never seen)
- `"text" + int` is pointer arithmetic (UB), use `<<` chaining or `std::to_string`
- `std::sort` comparator must be a strict weak ordering (`<`, never `<=`); fields fall through only when equal
- After a size mismatch, return early: the element loop would index past the shorter vector

Not covered

- Label image equality (bijection between my labels and OpenCV's); optional
- `i5-incoming`, and blobs touching the frame border (the Auckland masks may never produce one)

Open

- Fix the other path variable names in `ccl_check.cpp` when it moves to `ccl.hpp`; absolute macOS video paths are hardcoded
- Remaining Week 3: move the blob area filter out of `render`; check whether the 7x7 close merges two nearby cars
