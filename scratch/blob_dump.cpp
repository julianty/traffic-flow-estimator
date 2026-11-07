// Prints every kept blob's box for a range of frames, plus the gaps between
// nearby pairs, to size the fragment-grouping rule.
// Usage (run from the repo root):
//   build/blob_dump.exe <video> <start> [end=start] [scale=0.5] [out_dir=build/blobs]
// Runs the main pipeline from frame 1 (MOG2 -> threshold 200 -> open 3 -> close 7
// -> label (4-connectivity) -> area >= 200 -> nms 0.5 -> Tracker) so MOG2 and the
// tracker have their history, then for each frame in start..end:
//   - prints each blob: index, box (x, y, w, h), area, centroid, and the ID of the
//     track sitting on it this frame (- if none)
//   - prints every pair whose boxes are within pairGapLimit px in both x and y:
//       gapX / gapY = empty space between the boxes along that axis (0 if they overlap)
//       xOverlap    = shared x-extent / width of the narrower box (1.0 = one box's
//                     x-range sits inside the other's; 0 = no shared x-range)
//   - writes <out_dir>/<alias>-<frame>.png: the scaled frame with numbered boxes,
//     track IDs and the count line
// All numbers are in processing-scale pixels (same as the pipeline's constants).
// The pipeline constants are a manual copy of main.cpp's; keep them in sync.
#include "../src/ccl.hpp"
#include "../src/nms.hpp"
#include "../src/meta.hpp"
#include "../src/tracker.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {
    // Copy of main.cpp's pipeline constants
    constexpr int warmupFrames = 120;
    constexpr int mog2History = 500;
    constexpr double mog2VarThreshold = 16;
    constexpr double mog2LearningRate = 0.003;
    constexpr int blobAreaThreshold = 200;
    constexpr double iouThreshold = 0.5;
    constexpr int openKernelSize = 3;
    constexpr int closeKernelSize = 7;
    const std::string metadataPath = "source-videos/meta.json";

    // Pairs farther apart than this on either axis are not printed
    constexpr int pairGapLimit = 60;

    // Empty space between [a0, a1) and [b0, b1); 0 if they overlap or touch
    int gap(int a0, int a1, int b0, int b1) {
        return std::max(0, std::max(a0, b0) - std::min(a1, b1));
    }

    // Shared extent of [a0, a1) and [b0, b1); 0 if disjoint
    int overlap(int a0, int a1, int b0, int b1) {
        return std::max(0, std::min(a1, b1) - std::max(a0, b0));
    }

    // ID of the track that matched (or spawned on) this blob this frame, or -1
    int trackOn(const Blob& b, const std::vector<Track>& tracks) {
        for (const Track& t : tracks)
            if (t.missed == 0 && t.centroid == cv::Point2f(b.centroid)) return t.id;
        return -1;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <video> <start> [end=start] [scale=0.5] [out_dir=build/blobs]\n", argv[0]);
        return 1;
    }
    const std::string videoPath = argv[1];
    const int start = std::stoi(argv[2]);
    const int end = argc > 3 ? std::stoi(argv[3]) : start;
    const double scale = argc > 4 ? std::stod(argv[4]) : 0.5;
    const std::string outDir = argc > 5 ? argv[5] : "build/blobs";
    if (start < 1 || end < start || scale <= 0 || scale > 1) {
        std::fprintf(stderr, "need 1 <= start <= end and 0 < scale <= 1\n");
        return 1;
    }

    ClipMeta meta;
    try {
        meta = loadClipMeta(metadataPath, videoPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::fprintf(stderr, "cannot open %s\n", videoPath.c_str());
        return 1;
    }
    std::filesystem::create_directories(outDir);

    cv::Point lineA = meta.countLine.countLineA * scale;
    cv::Point lineB = meta.countLine.countLineB * scale;
    Tracker tracker{CountLine{lineA, lineB}};
    auto mog2 = cv::createBackgroundSubtractorMOG2(mog2History, mog2VarThreshold, true);
    cv::Mat openKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(openKernelSize, openKernelSize));
    cv::Mat closeKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(closeKernelSize, closeKernelSize));

    std::printf("clip %s, scale %.2f, count line y = %d (processing px)\n",
                meta.clipAlias.c_str(), scale, lineA.y);

    cv::Mat img, imgScaled, mask, binMask, filterMask, labels;
    std::vector<Blob> blobs;
    int frame = 0;
    while (frame < end && cap.read(img)) {
        frame++;
        if (scale == 1.0) imgScaled = img.clone();
        else cv::resize(img, imgScaled, cv::Size(), scale, scale, cv::INTER_AREA);
        mog2->apply(imgScaled, mask, mog2LearningRate);
        cv::threshold(mask, binMask, 200, 255, cv::THRESH_BINARY);
        cv::morphologyEx(binMask, filterMask, cv::MORPH_OPEN, openKernel);
        cv::morphologyEx(filterMask, filterMask, cv::MORPH_CLOSE, closeKernel);
        label(filterMask, labels, blobs, Neighbors::Four);
        blobs.erase(std::remove_if(blobs.begin(), blobs.end(),
                                   [](const Blob& b) { return b.area < blobAreaThreshold; }),
                    blobs.end());
        std::vector<Blob> kept = nms(blobs, iouThreshold);
        tracker.update(kept, frame > warmupFrames);

        if (frame < start) continue;

        const std::vector<Track>& tracks = tracker.tracks();
        std::printf("\n=== frame %d: %zu blobs, crossings so far %d\n", frame, kept.size(), tracker.count());
        std::printf("  idx      x     y     w     h   area    cx    cy  track\n");
        for (size_t i = 0; i < kept.size(); i++) {
            const Blob& b = kept[i];
            const cv::Rect& r = b.bounding_box;
            int id = trackOn(b, tracks);
            std::printf("  %3zu  %5d %5d %5d %5d %6d %5d %5d  %5s\n", i, r.x, r.y, r.width, r.height,
                        b.area, b.centroid.x, b.centroid.y, id < 0 ? "-" : std::to_string(id).c_str());
        }

        bool header = false;
        for (size_t i = 0; i < kept.size(); i++) {
            for (size_t j = i + 1; j < kept.size(); j++) {
                const cv::Rect& a = kept[i].bounding_box;
                const cv::Rect& b = kept[j].bounding_box;
                int gx = gap(a.x, a.x + a.width, b.x, b.x + b.width);
                int gy = gap(a.y, a.y + a.height, b.y, b.y + b.height);
                if (gx > pairGapLimit || gy > pairGapLimit) continue;
                int ox = overlap(a.x, a.x + a.width, b.x, b.x + b.width);
                double xOverlap = static_cast<double>(ox) / std::min(a.width, b.width);
                if (!header) {
                    std::printf("  pairs within %d px:  i - j   gapX  gapY  xOverlap\n", pairGapLimit);
                    header = true;
                }
                std::printf("                      %3zu - %-3zu %5d %5d  %8.2f\n", i, j, gx, gy, xOverlap);
            }
        }
        if (!header) std::printf("  no pairs within %d px\n", pairGapLimit);

        // Annotated image
        cv::Mat vis = imgScaled.clone();
        cv::line(vis, lineA, lineB, {0, 0, 255}, 1);
        for (size_t i = 0; i < kept.size(); i++) {
            const Blob& b = kept[i];
            cv::rectangle(vis, b.bounding_box, {255, 128, 0}, 1);
            int id = trackOn(b, tracks);
            std::string text = std::to_string(i) + (id < 0 ? "" : " t" + std::to_string(id));
            cv::putText(vis, text, b.bounding_box.tl() + cv::Point(1, -3), cv::FONT_HERSHEY_SIMPLEX,
                        0.4, {0, 255, 255}, 1);
        }
        cv::putText(vis, "Frame: " + std::to_string(frame), {10, vis.rows - 10}, cv::FONT_HERSHEY_SIMPLEX,
                    0.6, {0, 255, 0}, 1);
        std::string out = outDir + "/" + meta.clipAlias + "-" + std::to_string(frame) + ".png";
        cv::imwrite(out, vis);
    }
    if (frame < end) std::printf("\nclip ended at frame %d\n", frame);
    return 0;
}
