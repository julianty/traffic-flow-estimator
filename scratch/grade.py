#!/usr/bin/env python3
"""Grade pipeline crossing events against the manual ground truth and plot the result.

Usage (from the repo root; needs matplotlib, which the standalone Windows
Python 3.11 has and MSYS2's `python` does not):
    py -3.11 scratch/grade.py [--tol 15] [--events-dir build] [--out-dir build/grade]

For every clip in source-videos/meta.json whose annotations.ground_truth.file is
set, reads that CSV and build/events-<alias>.csv (written by
`main.exe --headless --event_out build/events-<alias>.csv`), matches them,
prints a summary plus every miss and false count, and writes four PNGs per clip
to --out-dir:
    <alias>-timeline.png       truth and pipeline crossings on two rows, matched pairs joined
    <alias>-offset.png         pipeline - truth frame for every matched crossing, vs. frame
    <alias>-offset-hist.png    distribution of those offsets
    <alias>-running-error.png  pipeline count - true count over the clip

Matching: greedy one-to-one, smallest |frame difference| first, within +-tol
frames, lanes ignored (the events have no lane yet). Offset = pipeline frame -
ground-truth frame, so negative means the pipeline counted earlier.
"""
import argparse
import csv
import json
import statistics
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # write files only, no window
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

META_PATH = Path("source-videos/meta.json")
WARMUP_FRAMES = 120  # same as main.cpp's warmupFrames; only used to shade the plots

# Colors (validated light-mode categorical slots 1-3 + neutral chrome); each kind
# also has its own marker shape so it never relies on color alone.
SURFACE = "#fcfcfb"
INK, INK_2, MUTED = "#0b0b0b", "#52514e", "#898781"
GRID, AXIS = "#e1e0d9", "#c3c2b7"
C_MATCHED, C_MISSED, C_FALSE = "#2a78d6", "#eb6834", "#1baf7a"
MATCHED = dict(marker="o", s=30, color=C_MATCHED, edgecolors=SURFACE, linewidths=1, zorder=3)
MISSED = dict(marker="X", s=80, color=C_MISSED, edgecolors=SURFACE, linewidths=1, zorder=4)
FALSE = dict(marker="^", s=70, color=C_FALSE, edgecolors=SURFACE, linewidths=1, zorder=4)


# ---------------------------------------------------------------- data + matching

def read_clip_csv(path, alias):
    """Rows of a per-clip CSV; every row's clip column must be this clip's alias."""
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    for line_no, row in enumerate(rows, start=2):
        if row.get("clip") != alias:
            sys.exit(f"{path}:{line_no}: clip is '{row.get('clip')}', expected '{alias}'")
    return rows


def match(gt_frames, ev_frames, tol):
    """Greedy one-to-one matching, nearest first. Returns (pairs, missed, false)."""
    candidates = sorted(
        (abs(e - g), gi, ei)
        for gi, g in enumerate(gt_frames)
        for ei, e in enumerate(ev_frames)
        if abs(e - g) <= tol
    )
    gt_used, ev_used, pairs = set(), set(), []
    for _, gi, ei in candidates:
        if gi in gt_used or ei in ev_used:
            continue
        gt_used.add(gi)
        ev_used.add(ei)
        pairs.append((gi, ei))
    pairs.sort()
    missed = [gi for gi in range(len(gt_frames)) if gi not in gt_used]
    false = [ei for ei in range(len(ev_frames)) if ei not in ev_used]
    return pairs, missed, false


def grade_clip(alias, clip_meta, gt_path, ev_path, tol):
    gt = read_clip_csv(gt_path, alias)
    ev = read_clip_csv(ev_path, alias)
    gt_frames = [int(r["frame"]) for r in gt]
    ev_frames = [int(r["frame"]) for r in ev]
    pairs, missed, false = match(gt_frames, ev_frames, tol)
    offsets = [ev_frames[ei] - gt_frames[gi] for gi, ei in pairs]
    tp = len(pairs)
    n_frames = round(clip_meta["duration_s"] * clip_meta["fps"])
    n_frames = max([n_frames] + gt_frames + ev_frames)
    return {
        "alias": alias, "gt": gt, "ev": ev, "gt_frames": gt_frames, "ev_frames": ev_frames,
        "pairs": pairs, "missed": missed, "false": false, "offsets": offsets, "tp": tp,
        "precision": tp / len(ev) if ev else float("nan"),
        "recall": tp / len(gt) if gt else float("nan"),
        "median_offset": statistics.median(offsets) if offsets else None,
        "n_frames": n_frames, "fps": clip_meta["fps"], "tol": tol,
    }


def nearest(frames, target, skip=None):
    """Index of the frame closest to target (optionally skipping one index)."""
    best = None
    for i, f in enumerate(frames):
        if i == skip:
            continue
        if best is None or abs(f - target) < abs(frames[best] - target):
            best = i
    return best


def error_rows(g):
    """(frame, kind, details, nearest) for every miss and false count, by frame."""
    rows = []
    for gi in g["missed"]:
        r = g["gt"][gi]
        ni = nearest(g["ev_frames"], g["gt_frames"][gi])
        near = (f"pipeline {g['ev_frames'][ni]} (track {g['ev'][ni]['track_id']}, x {g['ev'][ni]['x']}), "
                f"{g['ev_frames'][ni] - g['gt_frames'][gi]:+d} frames") if ni is not None else "-"
        rows.append((g["gt_frames"][gi], "Missed", f"lane {r['lane']}, {r['type']}", near))
    for ei in g["false"]:
        r = g["ev"][ei]
        ni = nearest(g["gt_frames"], g["ev_frames"][ei])
        oi = nearest(g["ev_frames"], g["ev_frames"][ei], skip=ei)
        near = []
        if ni is not None:
            near.append(f"truth {g['gt_frames'][ni]} (lane {g['gt'][ni]['lane']})")
        if oi is not None:
            near.append(f"other event {g['ev_frames'][oi]} (track {g['ev'][oi]['track_id']}, x {g['ev'][oi]['x']})")
        rows.append((g["ev_frames"][ei], "False count", f"track {r['track_id']}, x {r['x']}", "; ".join(near)))
    return sorted(rows)


# ---------------------------------------------------------------- plotting

def style_axes(ax, ygrid=True):
    ax.set_facecolor(SURFACE)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(AXIS)
    ax.tick_params(colors=MUTED, labelsize=9)
    ax.xaxis.label.set_color(INK_2)
    ax.yaxis.label.set_color(INK_2)
    if ygrid:
        ax.grid(axis="y", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)


def new_fig(height):
    fig, ax = plt.subplots(figsize=(10, height), dpi=150)
    fig.patch.set_facecolor(SURFACE)
    style_axes(ax)
    return fig, ax


def shade_warmup(ax):
    ax.axvspan(0, WARMUP_FRAMES, color=MUTED, alpha=0.12, linewidth=0, zorder=0)
    ax.text(WARMUP_FRAMES / 2, 1.0, "warm-up", transform=ax.get_xaxis_transform(),
            ha="center", va="bottom", fontsize=8, color=MUTED)


def title(ax, g, what):
    ax.set_title(f"{g['alias']}: {what}", loc="left", fontsize=11, color=INK, pad=14)


def save(fig, path):
    fig.tight_layout()
    fig.savefig(path, facecolor=SURFACE)
    plt.close(fig)


def plot_timeline(g, path):
    fig, ax = new_fig(2.6)
    ax.grid(False)
    shade_warmup(ax)
    gf, ef = g["gt_frames"], g["ev_frames"]
    y_gt, y_ev = 1, 0
    for gi, ei in g["pairs"]:
        ax.plot([gf[gi], ef[ei]], [y_gt, y_ev], color=MUTED, linewidth=0.7, alpha=0.6, zorder=1)
    ax.scatter([gf[gi] for gi, _ in g["pairs"]], [y_gt] * g["tp"], label="Matched", **MATCHED)
    ax.scatter([ef[ei] for _, ei in g["pairs"]], [y_ev] * g["tp"], **MATCHED)
    ax.scatter([gf[gi] for gi in g["missed"]], [y_gt] * len(g["missed"]),
               label=f"Missed ({len(g['missed'])})", **MISSED)
    ax.scatter([ef[ei] for ei in g["false"]], [y_ev] * len(g["false"]),
               label=f"False count ({len(g['false'])})", **FALSE)
    ax.set_yticks([y_ev, y_gt], ["Pipeline", "Ground truth"])
    ax.tick_params(axis="y", length=0, labelsize=10, labelcolor=INK_2)
    ax.spines["left"].set_visible(False)
    ax.set_ylim(-0.6, 1.6)
    ax.set_xlim(0, g["n_frames"])
    ax.set_xlabel("Frame")
    title(ax, g, f"crossing timeline (truth {len(gf)}, pipeline {len(ef)})")
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.32), ncol=3, frameon=False,
              fontsize=9, labelcolor=INK_2)
    save(fig, path)


def plot_offsets(g, path):
    fig, ax = new_fig(3.6)
    shade_warmup(ax)
    xs = [g["gt_frames"][gi] for gi, _ in g["pairs"]]
    ax.axhline(0, color=AXIS, linewidth=1, zorder=1)
    med = g["median_offset"]
    ax.axhline(med, color=INK_2, linewidth=1, linestyle=(0, (4, 3)), zorder=1)
    ax.text(g["n_frames"], med, f"median {med:g}", ha="right", va="bottom", fontsize=9, color=INK_2)
    ax.scatter(xs, g["offsets"], **MATCHED)
    ax.set_xlim(0, g["n_frames"])
    lo, hi = min(min(g["offsets"]), -1) - 1, max(max(g["offsets"]), 1) + 1
    ax.set_ylim(lo, hi)
    ax.yaxis.set_major_locator(MaxNLocator(integer=True))
    ax.set_xlabel("Ground-truth frame")
    ax.set_ylabel("Pipeline − truth (frames)")
    ms = 1000 / g["fps"]
    title(ax, g, f"crossing offset of the {g['tp']} matched crossings (1 frame = {ms:.1f} ms; "
                 f"below 0 = pipeline earlier)")
    save(fig, path)


def plot_histogram(g, path):
    fig, ax = new_fig(3.2)
    lo, hi = min(min(g["offsets"]), -1), max(max(g["offsets"]), 1)
    bins = list(range(lo, hi + 1))
    counts = [g["offsets"].count(b) for b in bins]
    ax.bar(bins, counts, width=0.85, color=C_MATCHED, zorder=2)
    ax.set_xticks(bins)
    ax.yaxis.set_major_locator(MaxNLocator(integer=True))
    ax.set_xlabel("Pipeline − truth (frames)")
    ax.set_ylabel("Matched crossings")
    within = sum(1 for o in g["offsets"] if abs(o) <= 3)
    title(ax, g, f"offset distribution (median {g['median_offset']:g}, "
                 f"{within}/{g['tp']} within ±3 frames, window ±{g['tol']})")
    save(fig, path)


def plot_running_error(g, path):
    fig, ax = new_fig(3.0)
    shade_warmup(ax)
    steps = sorted([(g["ev_frames"][ei], +1) for ei in g["false"]] +
                   [(g["gt_frames"][gi], -1) for gi in g["missed"]])
    xs, ys, cur = [0], [0], 0
    for f, d in steps:
        cur += d
        xs.append(f)
        ys.append(cur)
    xs.append(g["n_frames"])
    ys.append(cur)
    ax.axhline(0, color=AXIS, linewidth=1, zorder=1)
    ax.step(xs, ys, where="post", color=INK_2, linewidth=2, zorder=2)
    run = 0
    for f, d in steps:
        run += d
        ax.scatter([f], [run], **(FALSE if d > 0 else MISSED))
    ax.text(g["n_frames"], cur, f"end of clip: {cur:+d}", ha="right", va="bottom", fontsize=9, color=INK_2)
    lo, hi = min(min(ys), -1), max(max(ys), 1)
    ax.set_ylim(lo - 0.7, hi + 0.7)
    ax.yaxis.set_major_locator(MaxNLocator(integer=True))
    ax.set_xlim(0, g["n_frames"])
    ax.set_xlabel("Frame")
    ax.set_ylabel("Count error")
    ax.scatter([], [], label="False count (+1)", **FALSE)
    ax.scatter([], [], label="Missed (−1)", **MISSED)
    ax.legend(loc="upper left", frameon=False, fontsize=9, labelcolor=INK_2)
    title(ax, g, "running count error (pipeline − truth if the clip ended here)")
    save(fig, path)


# ---------------------------------------------------------------- main

def fmt(v):
    return "-" if v != v else f"{v:.3f}"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--tol", type=int, default=15, help="match window in frames (default 15)")
    ap.add_argument("--events-dir", default="build", help="directory holding events-<alias>.csv")
    ap.add_argument("--out-dir", default="build/grade", help="where the PNGs go")
    args = ap.parse_args()

    with open(META_PATH, encoding="utf-8") as f:
        clips = json.load(f)["clips"]
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    graded = 0
    for alias, meta in clips.items():
        gt_file = meta["annotations"]["ground_truth"].get("file")
        if not gt_file:
            continue
        ev_path = Path(args.events_dir) / f"events-{alias}.csv"
        if not ev_path.exists():
            print(f"skip {alias}: no {ev_path.as_posix()} (run main.exe --headless --scale 0.5 "
                  f"--event_out {ev_path.as_posix()})")
            continue
        g = grade_clip(alias, meta, Path(gt_file), ev_path, args.tol)
        graded += 1
        print(f"{alias}: truth {len(g['gt'])}, pipeline {len(g['ev'])}, matched {g['tp']}, "
              f"missed {len(g['missed'])}, false {len(g['false'])}, precision {fmt(g['precision'])}, "
              f"recall {fmt(g['recall'])}, median offset {g['median_offset']:g}")
        for f, kind, detail, near in error_rows(g):
            print(f"    {kind:<11} frame {f:>5}  {detail}  | nearest: {near}")
        plot_timeline(g, out_dir / f"{alias}-timeline.png")
        if g["offsets"]:
            plot_offsets(g, out_dir / f"{alias}-offset.png")
            plot_histogram(g, out_dir / f"{alias}-offset-hist.png")
        plot_running_error(g, out_dir / f"{alias}-running-error.png")

    if not graded:
        sys.exit("nothing to grade")
    print(f"plots in {out_dir.as_posix()}/")


if __name__ == "__main__":
    main()
