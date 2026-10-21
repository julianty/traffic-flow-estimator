// Shows MOG2 settling: how long until the ghosts of the first frame's vehicles are gone.
// Runs the same pipeline as main.cpp (resize -> MOG2 at the fixed rate -> threshold 200 ->
// open 3 -> close 7 -> label -> drop blobs under the area threshold) and writes three things:
//   <prefix>.csv        frame, foreground pixels, kept blobs for frames 1..plotFrames
//   <prefix>_grid.png   one row per snapshot frame: the color frame (left), the mask (right)
//   <prefix>_plot.png   foreground pixels vs. frame, with the baseline and the settling point
// and prints a warm-up estimate.
//
// Settling rule: the baseline is the median over frames [baselineStart, baselineEnd]. A metric has
// settled at the first frame f such that it stays <= tolerance * baseline for settleWindow frames
// in a row starting at f (so a single quiet frame in the ghost phase doesn't count).
// The reported warm-up is the later of the two metrics (fg pixels, blob count), plus a safety margin.
//
// Build: make build/warmup_check.exe
// Run from the repo root:
//   build/warmup_check.exe <video_path> [scale=0.5] [out_prefix=build/warmup]
// Frames count from 1, like the frame counter in main.
#include "../src/ccl.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>

namespace {
    // MOG2 and pipeline settings: keep in sync with main.cpp
    constexpr int mog2History = 500;
    constexpr double mog2VarThreshold = 16;
    constexpr double mog2LearningRate = 0.003;
    constexpr int fgThreshold = 200;
    constexpr int openKernelSize = 3;
    constexpr int closeKernelSize = 7;
    constexpr int blobAreaThreshold = 100;

    // Settling rule
    constexpr int baselineStart = 150;
    constexpr int baselineEnd = 300;
    constexpr double tolerance = 1.5;
    constexpr int settleWindow = 30;
    constexpr int safetyMargin = 15;

    // Outputs
    constexpr int plotFrames = 240;                         // CSV and plot cover frames 1..plotFrames
    const std::vector<int> snapshotFrames = {10, 50, 150};  // grid rows, top to bottom
    constexpr int plotWidth = 1400;
    constexpr int plotHeight = 700;
    constexpr int plotTickEvery = 20;                       // frames between x ticks

    double median(std::vector<int> v) {
        std::sort(v.begin(), v.end());
        size_t n = v.size();
        return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
    }

    // series[i] is frame i+1. Returns the first frame (1-based) from which the series stays
    // within limit for settleWindow frames, or -1 if it never does.
    int settleFrame(const std::vector<int>& series, double limit) {
        int n = static_cast<int>(series.size());
        for (int f = 0; f + settleWindow <= n; f++) {
            bool ok = true;
            for (int g = f; g < f + settleWindow && ok; g++) ok = series[g] <= limit;
            if (ok) return f + 1;
        }
        return -1;
    }

    void caption(cv::Mat& tile, const std::string& text) {
        double ratio = static_cast<double>(tile.rows) / 540;
        double fontScale = 0.9 * ratio;
        int wt = std::max(1, static_cast<int>(std::lround(2 * ratio)));
        int pad = static_cast<int>(std::lround(12 * ratio));
        int baseline = 0;
        cv::Size sz = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, fontScale, wt, &baseline);
        cv::putText(tile, text, {pad, pad + sz.height}, cv::FONT_HERSHEY_SIMPLEX, fontScale, {0, 255, 0}, wt);
    }

    // Rounds up to 1, 2, 5 or 10 times a power of ten
    int niceCeil(double v) {
        double mag = std::pow(10.0, std::floor(std::log10(v)));
        for (double m : {1.0, 2.0, 5.0, 10.0}) {
            if (m * mag >= v) return static_cast<int>(m * mag);
        }
        return static_cast<int>(10 * mag);
    }

    // Line plot of fg pixels vs. frame (frames 1..plotFrames), with the baseline, the tolerance limit,
    // the snapshot frames and the settling frame marked.
    cv::Mat drawPlot(const std::vector<int>& fgPixels, double baseline, double limit, int settle) {
        const int left = 120, right = 40, top = 70, bottom = 90;
        cv::Mat img(plotHeight, plotWidth, CV_8UC3, cv::Scalar(255, 255, 255));
        int pw = plotWidth - left - right, ph = plotHeight - top - bottom;
        int maxVal = *std::max_element(fgPixels.begin(), fgPixels.begin() + plotFrames);
        int yMax = niceCeil(std::max<double>(maxVal, limit) * 1.05);

        auto px = [&](double frame) { return left + static_cast<int>(std::lround((frame - 1) * pw / (plotFrames - 1))); };
        auto py = [&](double v) { return top + ph - static_cast<int>(std::lround(v * ph / yMax)); };
        const cv::Scalar grey(200, 200, 200), black(0, 0, 0);
        const int font = cv::FONT_HERSHEY_SIMPLEX;

        // Gridlines and tick labels
        for (int i = 0; i <= 5; i++) {
            int v = yMax * i / 5;
            cv::line(img, {left, py(v)}, {left + pw, py(v)}, grey, 1);
            cv::putText(img, std::to_string(v / 1000) + "k", {10, py(v) + 6}, font, 0.6, black, 1);
        }
        for (int f = 0; f <= plotFrames; f += plotTickEvery) {
            int x = px(std::max(f, 1));
            cv::line(img, {x, top}, {x, top + ph}, grey, 1);
            cv::putText(img, std::to_string(f), {x - 12, top + ph + 25}, font, 0.6, black, 1);
        }
        cv::rectangle(img, {left, top}, {left + pw, top + ph}, black, 1);

        // Baseline and limit
        cv::line(img, {left, py(baseline)}, {left + pw, py(baseline)}, {0, 140, 0}, 2);
        cv::line(img, {left, py(limit)}, {left + pw, py(limit)}, {0, 0, 200}, 2);
        cv::putText(img, "baseline (median, frames 150-300)", {left + 10, py(baseline) + 22}, font, 0.6, {0, 140, 0}, 1);
        cv::putText(img, "limit (" + std::to_string(tolerance).substr(0, 3) + "x baseline)", {left + 10, py(limit) - 8}, font, 0.6, {0, 0, 200}, 1);

        // Snapshot frames and the settling frame
        for (int f : snapshotFrames) {
            if (f > plotFrames) continue;
            cv::line(img, {px(f), top}, {px(f), top + ph}, {180, 120, 0}, 1, cv::LINE_AA);
            cv::putText(img, "snap " + std::to_string(f), {px(f) + 4, top + 16}, font, 0.5, {180, 120, 0}, 1);
        }
        if (settle > 0 && settle <= plotFrames) {
            cv::line(img, {px(settle), top}, {px(settle), top + ph}, {0, 0, 200}, 2, cv::LINE_AA);
            cv::putText(img, "settled: " + std::to_string(settle), {px(settle) + 6, top + ph - 10}, font, 0.6, {0, 0, 200}, 2);
        }

        // Data
        std::vector<cv::Point> pts;
        for (int f = 1; f <= plotFrames; f++) pts.push_back({px(f), py(fgPixels[f - 1])});
        cv::polylines(img, pts, false, {200, 90, 0}, 2, cv::LINE_AA);

        cv::putText(img, "Foreground pixels vs. frame (after threshold + morphology)", {left, 35}, font, 0.9, black, 2);
        cv::putText(img, "frame (25 fps: 240 frames = 9.6 s)", {left + pw / 2 - 170, plotHeight - 25}, font, 0.7, black, 1);
        return img;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <video_path> [scale=0.5] [out_prefix=build/warmup]\n";
        return 1;
    }
    std::string videoPath = argv[1];
    double scale = 0.5;
    std::string prefix = "build/warmup";
    try {
        if (argc > 2) scale = std::stod(argv[2]);
    } catch (const std::exception&) {
        std::cerr << "scale must be a number\n";
        return 1;
    }
    if (argc > 3) prefix = argv[3];
    if (scale <= 0.0 || scale > 1.0) {
        std::cerr << "need 0 < scale <= 1\n";
        return 1;
    }

    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Could not open " << videoPath << "\n";
        return 1;
    }
    FILE* csv = std::fopen((prefix + ".csv").c_str(), "w");
    if (!csv) {
        std::cerr << "Could not write " << prefix << ".csv (does the folder exist?)\n";
        return 1;
    }
    std::fprintf(csv, "frame,fg_pixels,blobs\n");

    auto mog2 = cv::createBackgroundSubtractorMOG2(mog2History, mog2VarThreshold, true);
    cv::Mat openKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(openKernelSize, openKernelSize));
    cv::Mat closeKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(closeKernelSize, closeKernelSize));

    std::vector<int> fgPixels, blobCounts;
    std::vector<cv::Mat> snapColor, snapMask;  // parallel to snapshotFrames
    cv::Mat img, imgScaled, mask, binMask, filterMask, labels;
    std::vector<Blob> blobs;
    while (static_cast<int>(fgPixels.size()) < baselineEnd && cap.read(img)) {
        if (scale == 1.0) imgScaled = img;
        else cv::resize(img, imgScaled, cv::Size(), scale, scale, cv::INTER_AREA);
        mog2->apply(imgScaled, mask, mog2LearningRate);
        cv::threshold(mask, binMask, fgThreshold, 255, cv::THRESH_BINARY);
        cv::morphologyEx(binMask, filterMask, cv::MORPH_OPEN, openKernel);
        cv::morphologyEx(filterMask, filterMask, cv::MORPH_CLOSE, closeKernel);
        label(filterMask, labels, blobs, Neighbors::Four);

        int kept = static_cast<int>(std::count_if(blobs.begin(), blobs.end(),
            [](const Blob& b) { return b.area >= blobAreaThreshold; }));
        fgPixels.push_back(cv::countNonZero(filterMask));
        blobCounts.push_back(kept);
        int frame = static_cast<int>(fgPixels.size());
        if (frame <= plotFrames) std::fprintf(csv, "%d,%d,%d\n", frame, fgPixels.back(), kept);

        if (std::find(snapshotFrames.begin(), snapshotFrames.end(), frame) != snapshotFrames.end()) {
            cv::Mat color = imgScaled.clone();
            cv::Mat maskBgr;
            cv::cvtColor(filterMask, maskBgr, cv::COLOR_GRAY2BGR);
            caption(color, "frame " + std::to_string(frame));
            caption(maskBgr, "mask: " + std::to_string(fgPixels.back()) + " fg px, " + std::to_string(kept) + " blobs");
            snapColor.push_back(color);
            snapMask.push_back(maskBgr);
        }
    }
    std::fclose(csv);
    if (static_cast<int>(fgPixels.size()) < baselineEnd || snapColor.size() != snapshotFrames.size()) {
        std::cerr << "Video ended at frame " << fgPixels.size() << ", too short for the baseline window\n";
        return 1;
    }

    auto window = [](const std::vector<int>& s) {
        return std::vector<int>(s.begin() + baselineStart - 1, s.begin() + baselineEnd);
    };
    double fgBase = median(window(fgPixels));
    double blobBase = median(window(blobCounts));
    // Blob counts are small integers, so give the limit a floor of baseline + 2
    double fgLimit = tolerance * fgBase;
    double blobLimit = std::max(tolerance * blobBase, blobBase + 2);
    int fgSettle = settleFrame(fgPixels, fgLimit);
    int blobSettle = settleFrame(blobCounts, blobLimit);

    // Grid: one row per snapshot, color on the left and mask on the right
    std::vector<cv::Mat> rows;
    for (size_t i = 0; i < snapColor.size(); i++) {
        cv::Mat row;
        cv::hconcat(std::vector<cv::Mat>{snapColor[i], snapMask[i]}, row);
        rows.push_back(row);
    }
    cv::Mat grid;
    cv::vconcat(rows, grid);
    bool ok = cv::imwrite(prefix + "_grid.png", grid);
    ok = cv::imwrite(prefix + "_plot.png", drawPlot(fgPixels, fgBase, fgLimit, fgSettle)) && ok;
    if (!ok) {
        std::cerr << "Could not write the PNGs under " << prefix << "\n";
        return 1;
    }

    std::printf("%s @ scale %.2f\n", videoPath.c_str(), scale);
    std::printf("wrote %s.csv, %s_grid.png (%dx%d), %s_plot.png\n", prefix.c_str(), prefix.c_str(), grid.cols, grid.rows, prefix.c_str());
    std::printf("baseline (median, frames %d-%d): fg pixels %.0f, blobs %.1f\n", baselineStart, baselineEnd, fgBase, blobBase);
    std::printf("settled (<= %.1fx baseline for %d frames in a row): fg pixels at frame %d (limit %.0f), "
                "blobs at frame %d (limit %.1f)\n", tolerance, settleWindow, fgSettle, fgLimit, blobSettle, blobLimit);
    if (fgSettle < 0 || blobSettle < 0) {
        std::printf("A metric never settled; check the CSV and the settling rule constants.\n");
        return 0;
    }
    int settle = std::max(fgSettle, blobSettle);
    std::printf("warm-up = later of the two (%d) + margin %d = %d frames\n", settle, safetyMargin, settle + safetyMargin);
    return 0;
}
