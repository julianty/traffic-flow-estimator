// Checks BlobMaxHeap against two references on random blobs:
//   1. the same blobs sorted with an independent copy of the ranking rule
//   2. std::priority_queue on area alone
// Build: make build/heap_check.exe   Run from the repo root.
#include "../src/heap.hpp"
#include <algorithm>
#include <cstdio>
#include <queue>
#include <random>
#include <vector>

namespace {
    // Independent copy of the ranking rule (heap.cpp's aLTb is file-private):
    // larger area ranks higher; ties go to the smaller y, then the smaller x.
    bool ranksAbove(const Blob& a, const Blob& b) {
        if (a.area != b.area) return a.area > b.area;
        if (a.bounding_box.y != b.bounding_box.y) return a.bounding_box.y < b.bounding_box.y;
        return a.bounding_box.x < b.bounding_box.x;
    }

    bool sameKey(const Blob& a, const Blob& b) {
        return a.area == b.area && a.bounding_box.y == b.bounding_box.y &&
               a.bounding_box.x == b.bounding_box.x;
    }

    // Small value ranges on purpose, so many blobs tie on area (and some on y and x too).
    std::vector<Blob> randomBlobs(std::mt19937& rng, int n) {
        std::uniform_int_distribution<int> areaDist(1, 8);
        std::uniform_int_distribution<int> posDist(0, 5);
        std::vector<Blob> blobs;
        for (int i = 0; i < n; i++) {
            Blob b;
            b.area = areaDist(rng);
            b.bounding_box = cv::Rect(posDist(rng), posDist(rng), 1, 1);
            b.centroid = cv::Point(0, 0);
            blobs.push_back(b);
        }
        return blobs;
    }

    int failures = 0;

    void fail(const char* what, int n, int trial) {
        std::printf("FAIL (%s): n=%d trial=%d\n", what, n, trial);
        failures++;
    }
}

int main() {
    std::mt19937 rng(187);
    const int sizes[] = {0, 1, 2, 3, 4, 5, 6, 7, 10, 31, 100, 1000};
    const int trials = 200;
    int checks = 0;

    for (int n : sizes) {
        for (int t = 0; t < trials; t++) {
            std::vector<Blob> blobs = randomBlobs(rng, n);

            std::vector<Blob> expected = blobs;
            std::sort(expected.begin(), expected.end(), ranksAbove);

            std::priority_queue<int> areaRef;
            for (const Blob& b : blobs) areaRef.push(b.area);

            BlobMaxHeap heap(blobs);
            if (heap.size() != blobs.size()) fail("size after build", n, t);
            if (heap.empty() != (n == 0)) fail("empty after build", n, t);

            for (int i = 0; i < n; i++) {
                if (heap.empty()) { fail("empty too early", n, t); break; }

                Blob top = heap.top();
                if (!sameKey(top, expected[i])) fail("pop order vs sorted", n, t);
                if (top.area != areaRef.top()) fail("area vs priority_queue", n, t);
                areaRef.pop();

                heap.pop();
                if (heap.size() != static_cast<size_t>(n - i - 1)) fail("size after pop", n, t);
                checks++;
            }
            if (!heap.empty()) fail("not empty at end", n, t);
        }
    }

    std::printf("%d pops checked, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
