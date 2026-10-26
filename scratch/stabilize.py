"""Lock a nearly-fixed handheld/tripod clip to its first frame (written by Claude).

usage: python3 scratch/stabilize.py <in.mp4> <out.mp4> [--ref N] [--crf 16]

Not a smoothing stabilizer: every frame is aligned to one reference frame, so slow
drift is removed too (that drift is what breaks MOG2). Per frame: ORB features at half
resolution, brute-force Hamming matches against the reference, RANSAC similarity
transform (rotation + uniform scale + translation). Frames with too few inliers reuse
the previous transform. Pass 1 estimates every transform; pass 2 warps and crops to the
rectangle that is valid in every frame, then pipes raw frames to ffmpeg (libx264).
"""
import argparse, subprocess, sys
import cv2
import numpy as np

DOWNSCALE = 0.5
MIN_INLIERS = 40


def gray_small(frame):
    small = cv2.resize(frame, None, fx=DOWNSCALE, fy=DOWNSCALE, interpolation=cv2.INTER_AREA)
    return cv2.cvtColor(small, cv2.COLOR_BGR2GRAY)


def estimate(path, ref_index):
    cap = cv2.VideoCapture(path)
    frames = []
    while True:
        ok, f = cap.read()
        if not ok:
            break
        frames.append(gray_small(f))
    orb = cv2.ORB_create(3000)
    matcher = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)
    ref_kp, ref_des = orb.detectAndCompute(frames[ref_index], None)

    transforms, inlier_counts, prev = [], [], np.eye(2, 3)
    for g in frames:
        kp, des = orb.detectAndCompute(g, None)
        A, n = None, 0
        if des is not None:
            m = matcher.match(des, ref_des)
            if len(m) >= MIN_INLIERS:
                src = np.float32([kp[x.queryIdx].pt for x in m])
                dst = np.float32([ref_kp[x.trainIdx].pt for x in m])
                A, inl = cv2.estimateAffinePartial2D(src, dst, method=cv2.RANSAC,
                                                     ransacReprojThreshold=1.5)
                n = int(inl.sum()) if inl is not None else 0
        if A is None or n < MIN_INLIERS:
            A = prev
        else:
            A = A.copy()
            A[:, 2] /= DOWNSCALE  # translation back to full resolution
        transforms.append(A)
        inlier_counts.append(n)
        prev = A
    return transforms, inlier_counts, cap.get(cv2.CAP_PROP_FPS)


def valid_crop(transforms, w, h):
    """Largest axis-aligned rect inside every warped frame (conservative: uses corners)."""
    x0, y0, x1, y1 = 0.0, 0.0, float(w), float(h)
    corners = np.float32([[0, 0], [w, 0], [w, h], [0, h]]).reshape(-1, 1, 2)
    for A in transforms:
        c = cv2.transform(corners, A).reshape(-1, 2)
        x0 = max(x0, c[0, 0], c[3, 0])
        x1 = min(x1, c[1, 0], c[2, 0])
        y0 = max(y0, c[0, 1], c[1, 1])
        y1 = min(y1, c[2, 1], c[3, 1])
    x0, y0 = int(np.ceil(x0)), int(np.ceil(y0))
    cw, ch = int(x1) - x0, int(y1) - y0
    return x0, y0, cw - cw % 2, ch - ch % 2  # even size for yuv420p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--ref", type=int, default=0, help="reference frame index")
    ap.add_argument("--crf", type=int, default=16)
    args = ap.parse_args()

    transforms, inliers, fps = estimate(args.input, args.ref)
    cap = cv2.VideoCapture(args.input)
    w, h = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)), int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    x, y, cw, ch = valid_crop(transforms, w, h)

    t = np.array([A[:, 2] for A in transforms])
    print(f"frames {len(transforms)}  fps {fps:.3f}", file=sys.stderr)
    print(f"correction dx [{t[:,0].min():.1f}, {t[:,0].max():.1f}]  "
          f"dy [{t[:,1].min():.1f}, {t[:,1].max():.1f}] px", file=sys.stderr)
    print(f"inliers min {min(inliers)}  median {int(np.median(inliers))}  "
          f"fallbacks {sum(n < MIN_INLIERS for n in inliers)}", file=sys.stderr)
    print(f"crop x={x} y={y} {cw}x{ch} (from {w}x{h})", file=sys.stderr)

    enc = subprocess.Popen(
        ["ffmpeg", "-hide_banner", "-v", "error", "-y",
         "-f", "rawvideo", "-pix_fmt", "bgr24", "-s", f"{cw}x{ch}", "-r", f"{fps}", "-i", "-",
         "-c:v", "libx264", "-crf", str(args.crf), "-preset", "medium", "-pix_fmt", "yuv420p",
         "-color_primaries", "bt709", "-color_trc", "bt709", "-colorspace", "bt709",
         args.output],
        stdin=subprocess.PIPE)
    for A in transforms:
        ok, f = cap.read()
        if not ok:
            break
        warped = cv2.warpAffine(f, A, (w, h), flags=cv2.INTER_LINEAR,
                                borderMode=cv2.BORDER_REPLICATE)
        enc.stdin.write(np.ascontiguousarray(warped[y:y + ch, x:x + cw]).tobytes())
    enc.stdin.close()
    sys.exit(enc.wait())


if __name__ == "__main__":
    main()
