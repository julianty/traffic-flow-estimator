// Times nms() (max-heap ordering) against brute_force_nms() (std::sort ordering),
// and checks that they return identical output.
//
// Part 1: real blobs from a clip (the same pipeline and clip as ccl_bench), the case the
//         project actually runs.
// Part 2: synthetic blobs at growing n, to show where (and whether) the heap starts to matter.
//         Sparse = most boxes survive, so the shared keep check dominates;
//         dense  = most boxes are suppressed, so ordering dominates.
//
// Usage (run from the repo root):
//   build/nms_bench.exe [video=source-videos/ucsd-i5-incoming-stabilized-boosted-dslr.mp4]
//                       [scale=0.5] [frames=300] [skip=121]
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
#include "../src/ccl.hpp"
#include "../src/nms.hpp"

namespace {
    constexpr int openKernelSize = 3;
    constexpr int closeKernelSize = 7;
    constexpr int blobAreaThreshold = 200;  // same as src/main.cpp (0.5-scale px)
    constexpr double iouThreshold = 0.5;    // same as src/main.cpp

    using Clock = std::chrono::steady_clock;
    volatile size_t sink = 0;  // keeps the optimizer from deleting the timed calls

    bool sameBlobs(const std::vector<Blob>& a, const std::vector<Blob>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++) {
            if (a[i].bounding_box != b[i].bounding_box || a[i].area != b[i].area ||
                a[i].centroid != b[i].centroid) return false;
        }
        return true;
    }

    // Total microseconds for `reps` calls of f on `blobs`
    template <typename F>
    double timeCalls(F f, const std::vector<Blob>& blobs, int reps) {
        auto t0 = Clock::now();
        for (int r = 0; r < reps; r++) sink += f(blobs, iouThreshold).size();
        return std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
    }

    // n random blobs with distinct (area, y, x): the area field is a shuffled 1..n, so ties
    // in the ranking rule cannot make the two functions legitimately differ.
    // canvas = side length of the square the boxes are scattered over.
    std::vector<Blob> makeBlobs(int n, int canvas, std::mt19937& rng) {
        std::vector<int> areas(n);
        std::iota(areas.begin(), areas.end(), 1);
        std::shuffle(areas.begin(), areas.end(), rng);
        std::uniform_int_distribution<int> pos(0, canvas), size(10, 60);
        std::vector<Blob> blobs;
        blobs.reserve(n);
        for (int i = 0; i < n; i++) {
            cv::Rect box(pos(rng), pos(rng), size(rng), size(rng));
            blobs.push_back({box, areas[i], cv::Point(box.x + box.width / 2, box.y + box.height / 2)});
        }
        return blobs;
    }

    void realBlobs(const std::string& path, double scale, int numFrames, int skip) {
        cv::VideoCapture cap(path);
        if (!cap.isOpened()) {
            std::cerr << "Could not open " << path << "\n";
            std::exit(1);
        }
        cv::Ptr<cv::BackgroundSubtractorMOG2> mog2 = cv::createBackgroundSubtractorMOG2(500, 16.0, true);
        cv::Mat openK = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(openKernelSize, openKernelSize));
        cv::Mat closeK = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(closeKernelSize, closeKernelSize));
        cv::Mat img, scaled, mask, bin, filt, labels;
        std::vector<Blob> blobs;
        std::vector<std::vector<Blob>> raw, filtered;  // per frame, before / after the area filter
        int frame = 0;
        while ((int)raw.size() < numFrames && cap.read(img)) {
            frame++;
            if (scale == 1.0) scaled = img;
            else cv::resize(img, scaled, cv::Size(), scale, scale, cv::INTER_AREA);
            mog2->apply(scaled, mask, 0.003);
            if (frame < skip) continue;
            cv::threshold(mask, bin, 200, 255, cv::THRESH_BINARY);
            cv::morphologyEx(bin, filt, cv::MORPH_OPEN, openK);
            cv::morphologyEx(filt, filt, cv::MORPH_CLOSE, closeK);
            label(filt, labels, blobs, Neighbors::Four);
            raw.push_back(blobs);
            std::vector<Blob> keptBlobs = blobs;
            keptBlobs.erase(std::remove_if(keptBlobs.begin(), keptBlobs.end(),
                                           [](const Blob& b) { return b.area < blobAreaThreshold; }),
                            keptBlobs.end());
            filtered.push_back(keptBlobs);
        }

        std::cout << "== Part 1: real blobs ==\nClip: " << path << ", scale " << scale << ", "
                  << raw.size() << " frames (" << skip << ".." << skip + (int)raw.size() - 1
                  << "), IoU threshold " << iouThreshold << "\n";
        const int reps = 2000;  // one nms call on ~4 blobs is far below timer resolution
        for (int pass = 0; pass < 2; pass++) {
            const auto& set = pass == 0 ? raw : filtered;
            long long nTotal = 0, keptTotal = 0;
            int maxN = 0, mismatches = 0;
            double usHeap = 0, usSort = 0;
            for (const auto& b : set) {
                nTotal += (long long)b.size();
                maxN = std::max(maxN, (int)b.size());
                auto a = nms(b, iouThreshold), s = brute_force_nms(b, iouThreshold);
                keptTotal += (long long)a.size();
                if (!sameBlobs(a, s)) mismatches++;
                usHeap += timeCalls(nms, b, reps);
                usSort += timeCalls(brute_force_nms, b, reps);
            }
            long long calls = (long long)set.size() * reps;
            std::cout << "\n" << (pass == 0 ? "Before area filter" : "After area filter (area >= 200)")
                      << ": " << (double)nTotal / set.size() << " blobs/frame avg (max " << maxN
                      << "), " << (double)keptTotal / set.size() << " kept/frame\n"
                      << "  heap vs sort output mismatches: " << mismatches << "/" << set.size() << "\n"
                      << "  nms() heap        : " << usHeap / calls << " us/call\n"
                      << "  sort-based version: " << usSort / calls << " us/call\n";
        }
    }

    void synthetic() {
        std::cout << "\n== Part 2: synthetic blobs (IoU threshold " << iouThreshold << ") ==\n"
                  << "sparse = boxes spread out, most survive; dense = boxes piled up, most suppressed\n";
        std::mt19937 rng(12345);
        for (const char* shape : {"sparse", "dense"}) {
            std::cout << "\n" << shape << "\n   n     kept   heap (us)   sort (us)   outputs equal\n";
            for (int n : {10, 100, 1000, 10000}) {
                // sparse: constant density as n grows; dense: fixed 300x300 canvas
                int canvas = shape[0] == 's' ? (int)(80 * std::sqrt((double)n)) : 300;
                std::vector<Blob> blobs = makeBlobs(n, canvas, rng);
                int reps = std::max(1, 20000 / n);
                auto a = nms(blobs, iouThreshold), s = brute_force_nms(blobs, iouThreshold);
                double usHeap = timeCalls(nms, blobs, reps) / reps;
                double usSort = timeCalls(brute_force_nms, blobs, reps) / reps;
                std::cout << "  " << n << "\t" << a.size() << "\t" << usHeap << "\t" << usSort << "\t"
                          << (sameBlobs(a, s) ? "yes" : "NO") << "\n";
            }
        }
    }
}

int main(int argc, char* argv[]) {
    std::string path = argc > 1 ? argv[1] : "source-videos/ucsd-i5-incoming-stabilized-boosted-dslr.mp4";
    double scale = argc > 2 ? std::stod(argv[2]) : 0.5;
    int numFrames = argc > 3 ? std::stoi(argv[3]) : 300;
    int skip = argc > 4 ? std::stoi(argv[4]) : 121;
    realBlobs(path, scale, numFrames, skip);
    synthetic();
    return 0;
}
