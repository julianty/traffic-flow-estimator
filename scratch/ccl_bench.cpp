// Times label() (BFS), brute_force_label() (label propagation) and
// cv::connectedComponentsWithStats on the same pre-computed masks, and checks
// that BFS and brute force agree exactly (label image + blobs, same order).
//
// Usage (run from the repo root):
//   build/ccl_bench.exe [video=source-videos/ucsd-i5-incoming-stabilized-boosted-dslr.mp4]
//                       [scale=0.5] [frames=100] [skip=121]
// MOG2 / threshold / morphology run once up front and are NOT timed; only the
// labeling calls are inside the stopwatches.
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
#include "../src/ccl.hpp"

namespace {
    // Same constants as src/main.cpp
    constexpr int openKernelSize = 3;
    constexpr int closeKernelSize = 7;

    using Clock = std::chrono::steady_clock;
    double msSince(Clock::time_point t0) {
        return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    }

    struct Timing {
        double total = 0.0;
        double worst = 0.0;
        void add(double ms) { total += ms; worst = std::max(worst, ms); }
    };

    bool sameBlobs(const std::vector<Blob>& a, const std::vector<Blob>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++) {
            if (a[i].bounding_box != b[i].bounding_box) return false;
            if (a[i].area != b[i].area) return false;
            if (a[i].centroid != b[i].centroid) return false;
        }
        return true;
    }

    void report(const char* name, const Timing& t, int n) {
        std::cout << "  " << name << ": total " << t.total << " ms, mean " << t.total / n
                  << " ms/frame, worst " << t.worst << " ms\n";
    }
}

int main(int argc, char* argv[]) {
    std::string path = argc > 1 ? argv[1] : "source-videos/ucsd-i5-incoming-stabilized-boosted-dslr.mp4";
    double scale = argc > 2 ? std::stod(argv[2]) : 0.5;
    int numFrames = argc > 3 ? std::stoi(argv[3]) : 100;
    int skip = argc > 4 ? std::stoi(argv[4]) : 121;  // first frame to keep (1-based)

    cv::VideoCapture cap(path);
    if (!cap.isOpened()) {
        std::cerr << "Could not open " << path << "\n";
        return 1;
    }

    // ---- Build the masks (untimed) ----
    cv::Ptr<cv::BackgroundSubtractorMOG2> mog2 = cv::createBackgroundSubtractorMOG2(500, 16.0, true);
    cv::Mat openKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(openKernelSize, openKernelSize));
    cv::Mat closeKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(closeKernelSize, closeKernelSize));
    std::vector<cv::Mat> masks;
    cv::Mat img, scaled, mask, binMask, filtered;
    int frame = 0;
    long long fgPixels = 0;
    while ((int)masks.size() < numFrames && cap.read(img)) {
        frame++;
        if (scale == 1.0) scaled = img;
        else cv::resize(img, scaled, cv::Size(), scale, scale, cv::INTER_AREA);
        mog2->apply(scaled, mask, 0.003);  // same fixed rate as main
        if (frame < skip) continue;
        cv::threshold(mask, binMask, 200, 255, cv::THRESH_BINARY);
        cv::morphologyEx(binMask, filtered, cv::MORPH_OPEN, openKernel);
        cv::morphologyEx(filtered, filtered, cv::MORPH_CLOSE, closeKernel);
        masks.push_back(filtered.clone());
        fgPixels += cv::countNonZero(filtered);
    }
    int n = (int)masks.size();
    if (n == 0) {
        std::cerr << "No frames collected\n";
        return 1;
    }
    std::cout << "Clip: " << path << "\nScale " << scale << ", " << n << " masks (frames " << skip << ".."
              << skip + n - 1 << "), " << masks[0].cols << "x" << masks[0].rows
              << ", mean foreground " << fgPixels / n << " px\n";

    // ---- Time and verify, per connectivity ----
    cv::Mat labelsBfs, labelsBf, cvLabels, stats, centroids;
    std::vector<Blob> blobsBfs, blobsBf;
    bool allOk = true;
    for (int kn : {4, 8}) {
        Neighbors nb = kn == 4 ? Neighbors::Four : Neighbors::Eight;
        Timing tBfs, tBf, tCv;
        int mismatches = 0;
        long long blobTotal = 0;
        for (const cv::Mat& m : masks) {
            auto t0 = Clock::now();
            label(m, labelsBfs, blobsBfs, nb);
            tBfs.add(msSince(t0));

            t0 = Clock::now();
            brute_force_label(m, labelsBf, blobsBf, nb);
            tBf.add(msSince(t0));

            t0 = Clock::now();
            cv::connectedComponentsWithStats(m, cvLabels, stats, centroids, kn, CV_32S);
            tCv.add(msSince(t0));

            bool ok = cv::countNonZero(labelsBfs != labelsBf) == 0 && sameBlobs(blobsBfs, blobsBf);
            if (!ok) mismatches++;
            blobTotal += (long long)blobsBfs.size();
        }
        std::cout << "\n" << kn << "-connectivity (" << blobTotal / n << " blobs/frame avg), "
                  << "BFS vs brute-force mismatches: " << mismatches << "/" << n << "\n";
        report("BFS label()         ", tBfs, n);
        report("brute_force_label() ", tBf, n);
        report("cv::connectedComp.  ", tCv, n);
        std::cout << "  brute force / BFS = " << tBf.total / tBfs.total << "x\n";
        if (mismatches) allOk = false;
    }
    return allOk ? 0 : 2;
}
