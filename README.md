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

- C++17, g++ (MinGW-w64 on Windows, Apple clang on macOS)
- OpenCV 5.0.0 (video I/O, background subtraction, basic preprocessing)
- GNU Make + pkg-config

## Setup

The build works on Windows (MSYS2) and macOS (Homebrew). The `Makefile` gets OpenCV's include and library paths from `pkg-config opencv5`, so the same file works on both.

### Windows (MSYS2 UCRT64)

1. Install [MSYS2](https://www.msys2.org/) and open the **UCRT64** terminal.
2. Install the toolchain, OpenCV, and Qt6:
   ```sh
   pacman -S make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pkgconf \
             mingw-w64-ucrt-x86_64-opencv mingw-w64-ucrt-x86_64-qt6-5compat
   ```
   pacman lists Qt6 as optional, but MSYS2's HighGUI (`imshow`) is built on Qt6. Without it, executables exit silently with code 127 (missing DLLs).
3. To run executables outside the UCRT64 terminal, add `C:\msys64\ucrt64\bin` to the Windows `PATH`. From Git Bash, add `/c/msys64/ucrt64/bin` instead.

### macOS (Homebrew)

1. Install the Xcode command-line tools (`xcode-select --install`) and [Homebrew](https://brew.sh/). `g++` on macOS is Apple clang, which supports C++17.
2. Install OpenCV:
   ```sh
   brew install openssl@4   # install first, see note below
   brew install opencv
   ```
   pkg-config comes in as a dependency. HighGUI uses the native macOS windowing, so you don't need extra setup for `imshow`.

   > **Note (Homebrew 7.0.9, Oct 2026):** a plain `brew install opencv` can fail with *"A `brew install opencv` process has already locked /opt/homebrew/Cellar/openssl@3"*. No other brew process is involved: opencv's dependency tree includes both `openssl@3` and `openssl@4`, and installing `openssl@4` makes brew re-lock `openssl@3`, which is already locked. Installing `openssl@4` on its own first avoids this. If an install was interrupted, run `brew reinstall openssl@4` and then `brew install opencv`.

### Check the install

```sh
pkg-config --modversion opencv5          # should print 5.0.0
make build/hello_opencv.exe && build/hello_opencv.exe
```

A small window saying "it works" should appear. Press any key to close it.

### Build and run

```sh
make                                     # builds build/main.exe
build/main.exe --video_path source-videos/16516296_1920_1080_25fps.mp4 --scale 0.5
```

- `--scale <s>` (`0 < s <= 1`) downscales frames before processing. At 1.0, the side-by-side display is 3840 px wide, so use 0.5 when watching interactively.
- `--headless` skips all display, for timing runs.
- While the window is open: space pauses and resumes; `q`/Esc quits.
- The `.exe` suffix is kept on macOS too, so targets and commands match on both platforms.

### VS Code build shortcut (optional)

`.vscode/` is gitignored, so each machine needs its own `tasks.json`: a `shell` task running `make` in `${workspaceFolder}`, marked as the default build task (`"group": {"kind": "build", "isDefault": true}`), with `"problemMatcher": ["$gcc"]`.

- **Windows:** run it through MSYS2 bash, with `options.shell.executable` set to `C:\msys64\usr\bin\bash.exe`, args `["--login", "-c"]`, and env `MSYSTEM=UCRT64`, `CHERE_INVOKING=1`. Shortcut: Ctrl+Shift+B.
- **macOS:** plain `make` is enough. Prepend `/opt/homebrew/bin` to `PATH` in `options.env`, so pkg-config is found even when VS Code isn't launched from a terminal. Shortcut: Cmd+Shift+B.

## Data

Source footage is fixed-camera traffic video from [Pexels](https://www.pexels.com/), downloaded by hand. Each clip was checked for camera motion, since background subtraction needs a static camera. Per-clip metadata (source URL, resolution/fps, stability, role) lives in `source-videos/meta.json`: the main clip is `auckland-hwy`, the validation clip is `auckland-fwy`, and the backup is `urban-road`.

Clips are used under the [Pexels License](https://www.pexels.com/license/) (free to use and modify, no attribution required, no redistributing unaltered copies), so the raw video is not checked into this repo (see `.gitignore`).

## Benchmark

Pipeline vehicle counts will be validated against a manually-counted ground truth on the same footage.
