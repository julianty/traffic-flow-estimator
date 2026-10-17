// Checks nms() with hand-made cases, then with random blobs against two properties
// of greedy NMS (survivors don't overlap above the threshold; every dropped blob
// overlaps a survivor above the threshold).
// Build: make build/nms_check.exe   Run from the repo root.
#include "../src/nms.hpp"
#include <cstdio>
#include <random>
#include <vector>

namespace {
    int failures = 0;

    void check(bool ok, const char* name) {
        if (!ok) {
            failures++;
            std::printf("FAIL: %s\n", name);
        }
    }

    Blob mk(int x, int y, int w, int h, int area) {
        Blob b;
        b.bounding_box = cv::Rect(x, y, w, h);
        b.area = area;
        b.centroid = cv::Point(x + w / 2, y + h / 2);
        return b;
    }

    // Independent IoU via cv::Rect's & operator (nms.cpp's iouScore is file-private).
    double iou(const cv::Rect& a, const cv::Rect& b) {
        double inter = (a & b).area();
        double uni = a.area() + b.area() - inter;
        return uni <= 0 ? 0 : inter / uni;
    }

    void handMade() {
        // 1. empty in, empty out
        check(nms({}, 0.5).empty(), "empty input");

        // 2. identical boxes: the larger-area blob survives
        {
            auto out = nms({mk(0, 0, 10, 10, 90), mk(0, 0, 10, 10, 100)}, 0.5);
            check(out.size() == 1 && out[0].area == 100, "identical boxes keep the larger");
        }

        // 3. disjoint boxes both survive
        {
            auto out = nms({mk(0, 0, 10, 10, 100), mk(50, 50, 10, 10, 90)}, 0.0);
            check(out.size() == 2, "disjoint boxes both survive");
        }

        // 4. edge-touching boxes both survive (zero intersection area), even at threshold 0
        {
            auto out = nms({mk(0, 0, 10, 10, 100), mk(10, 0, 10, 10, 90)}, 0.0);
            check(out.size() == 2, "touching boxes both survive");
        }

        // 5. chain A-B-C: A overlaps B, B overlaps C, A and C disjoint.
        //    IoU(A,B) = IoU(B,C) = 40/160 = 0.25. B is dropped by A; C must still survive
        //    because the dropped B does not suppress anything.
        {
            std::vector<Blob> in = {mk(0, 0, 10, 10, 100), mk(6, 0, 10, 10, 90), mk(12, 0, 10, 10, 80)};
            auto out = nms(in, 0.2);
            check(out.size() == 2 && out[0].area == 100 && out[1].area == 80, "chain keeps A and C");

            // same result when the input order is reversed
            auto rev = nms({in[2], in[1], in[0]}, 0.2);
            check(rev.size() == 2 && rev[0].area == 100 && rev[1].area == 80, "chain, reversed input");
        }

        // 6. threshold boundary: IoU is exactly 0.25. Suppress only when strictly greater.
        {
            std::vector<Blob> in = {mk(0, 0, 10, 10, 100), mk(6, 0, 10, 10, 90)};
            check(nms(in, 0.25).size() == 2, "IoU == threshold is kept");
            check(nms(in, 0.24).size() == 1, "IoU just above threshold is dropped");
        }

        // 7. threshold 1.0 keeps even identical boxes (IoU 1.0 is not > 1.0)
        {
            auto out = nms({mk(0, 0, 10, 10, 100), mk(0, 0, 10, 10, 90)}, 1.0);
            check(out.size() == 2, "threshold 1.0 keeps everything");
        }

        // 8. output is ordered by rank, largest first
        {
            auto out = nms({mk(0, 0, 5, 5, 25), mk(50, 0, 10, 10, 100), mk(100, 0, 8, 8, 64)}, 0.5);
            check(out.size() == 3 && out[0].area == 100 && out[1].area == 64 && out[2].area == 25,
                  "output ordered by area");
        }
    }

    void randomized() {
        std::mt19937 rng(12345);
        std::uniform_int_distribution<int> nDist(0, 30);
        std::uniform_int_distribution<int> posDist(0, 60);
        std::uniform_int_distribution<int> sizeDist(1, 25);
        std::uniform_real_distribution<double> thrDist(0.0, 1.0);

        for (int trial = 0; trial < 2000; trial++) {
            int n = nDist(rng);
            double t = thrDist(rng);
            std::vector<Blob> in;
            for (int i = 0; i < n; i++) {
                int w = sizeDist(rng), h = sizeDist(rng);
                in.push_back(mk(posDist(rng), posDist(rng), w, h, w * h));
            }
            auto out = nms(in, t);

            // survivors never overlap above the threshold
            bool pairsOk = true;
            for (size_t i = 0; i < out.size(); i++)
                for (size_t j = i + 1; j < out.size(); j++)
                    if (iou(out[i].bounding_box, out[j].bounding_box) > t) pairsOk = false;
            check(pairsOk, "random: survivors overlap above threshold");

            // every input blob is either a survivor or overlaps one above the threshold
            bool coveredOk = true;
            for (const Blob& b : in) {
                bool isKept = false, suppressed = false;
                for (const Blob& k : out) {
                    if (k.bounding_box == b.bounding_box && k.area == b.area) isKept = true;
                    if (iou(k.bounding_box, b.bounding_box) > t) suppressed = true;
                }
                if (!isKept && !suppressed) coveredOk = false;
            }
            check(coveredOk, "random: dropped blob not covered by a survivor");

            // the largest blob always survives; nothing is invented
            if (n > 0) {
                int maxArea = 0;
                for (const Blob& b : in) maxArea = std::max(maxArea, b.area);
                check(!out.empty() && out[0].area == maxArea, "random: largest blob first");
            }
            check(out.size() <= in.size(), "random: output larger than input");
        }
    }
}

int main() {
    handMade();
    randomized();
    if (failures == 0) std::printf("all nms checks passed\n");
    else std::printf("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
