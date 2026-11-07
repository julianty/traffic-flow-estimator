// Checks groupFragments on hand cases (including real boxes from the grade's
// double counts) plus random inputs.
// Rule under test: unite two blobs if xOverlap >= minXOverlap (shared x-extent /
// narrower width) and gapY <= maxGapY (both inclusive).
// Build: make build/group_check.exe   Run from anywhere.
#include "../src/group.hpp"
#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>

namespace {
    constexpr double minXOverlap = 0.5;
    constexpr int maxGapY = 10;

    int failures = 0;
    int checks = 0;

    void expect(bool ok, const char* what) {
        checks++;
        if (!ok) {
            failures++;
            std::printf("FAIL: %s\n", what);
        }
    }

    Blob blob(int x, int y, int w, int h, int area, int cx, int cy) {
        return Blob{cv::Rect(x, y, w, h), area, cv::Point(cx, cy)};
    }

    // Blob whose centroid is the box center and area is the box area
    Blob boxBlob(int x, int y, int w, int h) {
        return blob(x, y, w, h, w * h, x + w / 2, y + h / 2);
    }

    bool sameBlob(const Blob& a, const Blob& b) {
        return a.bounding_box == b.bounding_box && a.area == b.area && a.centroid == b.centroid;
    }

    void printBlob(const char* label, const Blob& b) {
        const cv::Rect& r = b.bounding_box;
        std::printf("    %s: box (%d, %d, %d, %d) area %d centroid (%d, %d)\n", label, r.x, r.y, r.width,
                    r.height, b.area, b.centroid.x, b.centroid.y);
    }

    void expectOne(const std::vector<Blob>& out, const Blob& want, const char* what) {
        bool ok = out.size() == 1 && sameBlob(out[0], want);
        expect(ok, what);
        if (!ok) {
            std::printf("    got %zu blobs\n", out.size());
            for (const Blob& b : out) printBlob("got", b);
            printBlob("want", want);
        }
    }

    void handCases() {
        // Empty in, empty out
        expect(groupFragments({}, minXOverlap, maxGapY).empty(), "empty input");

        // A single blob comes back unchanged
        {
            Blob b = blob(10, 20, 30, 40, 900, 25, 41);
            std::vector<Blob> out = groupFragments({b}, minXOverlap, maxGapY);
            expect(out.size() == 1 && sameBlob(out[0], b), "single blob unchanged");
        }

        // Case B (ucsd-i5-incoming-dslr frame 1667, @0.5): truck + rack box inside it.
        // Union box = the truck's box; area 10771 + 230 = 11001;
        // centroid x = (10771*290 + 230*323) / 11001 = 290.69 -> 291
        //          y = (10771*203 + 230*147) / 11001 = 201.83 -> 202
        {
            std::vector<Blob> in = {blob(239, 96, 112, 176, 10771, 290, 203),
                                    blob(309, 136, 30, 22, 230, 323, 147)};
            expectOne(groupFragments(in, minXOverlap, maxGapY), blob(239, 96, 112, 176, 11001, 291, 202),
                      "case B: box inside box -> one blob");
        }

        // Case A (ucsd-i5-incoming-dslr frame 1206, @0.5): stacked halves of one car.
        // xOverlap = 64/65 = 0.985, gapY = 181 - 177 = 4.
        // Union box (663, 145, 66, 78); area 3104;
        // centroid x = (2056*697 + 1048*695) / 3104 = 696.32 -> 696
        //          y = (2056*203 + 1048*165) / 3104 = 190.17 -> 190
        {
            std::vector<Blob> in = {blob(664, 181, 65, 42, 2056, 697, 203),
                                    blob(663, 145, 65, 32, 1048, 695, 165)};
            expectOne(groupFragments(in, minXOverlap, maxGapY), blob(663, 145, 66, 78, 3104, 696, 190),
                      "case A: stacked pair (xOverlap 0.985, gapY 4) -> one blob");
        }

        // Neighbouring lanes, same frame 1206: xOverlap 0, gapX 4 -> stay apart
        {
            std::vector<Blob> in = {blob(380, 130, 71, 100, 5073, 414, 184),
                                    blob(325, 45, 51, 58, 2068, 347, 75)};
            std::vector<Blob> out = groupFragments(in, minXOverlap, maxGapY);
            expect(out.size() == 2, "neighbouring lanes (xOverlap 0) stay apart");
        }

        // Side by side, level in y, small x-overlap (10 / 50 = 0.2) -> stay apart
        {
            std::vector<Blob> in = {boxBlob(100, 100, 50, 40), boxBlob(140, 100, 50, 40)};
            expect(groupFragments(in, minXOverlap, maxGapY).size() == 2,
                   "side by side, xOverlap 0.2 -> stay apart");
        }

        // Threshold boundaries (both inclusive)
        {
            // gapY exactly maxGapY -> merge; maxGapY + 1 -> apart
            std::vector<Blob> atGap = {boxBlob(100, 0, 50, 20), boxBlob(100, 20 + maxGapY, 50, 20)};
            std::vector<Blob> pastGap = {boxBlob(100, 0, 50, 20), boxBlob(100, 21 + maxGapY, 50, 20)};
            expect(groupFragments(atGap, minXOverlap, maxGapY).size() == 1, "gapY == maxGapY merges");
            expect(groupFragments(pastGap, minXOverlap, maxGapY).size() == 2, "gapY == maxGapY + 1 stays apart");

            // xOverlap exactly 0.5 (25 / 50) -> merge; 24 / 50 -> apart
            std::vector<Blob> atOverlap = {boxBlob(100, 0, 50, 20), boxBlob(125, 20, 50, 20)};
            std::vector<Blob> underOverlap = {boxBlob(100, 0, 50, 20), boxBlob(126, 20, 50, 20)};
            expect(groupFragments(atOverlap, minXOverlap, maxGapY).size() == 1, "xOverlap == 0.5 merges");
            expect(groupFragments(underOverlap, minXOverlap, maxGapY).size() == 2, "xOverlap 0.48 stays apart");

            // xOverlap uses the narrower width: a 20-wide box fully under a 100-wide one is 1.0
            std::vector<Blob> narrow = {boxBlob(0, 0, 100, 20), boxBlob(40, 25, 20, 20)};
            expect(groupFragments(narrow, minXOverlap, maxGapY).size() == 1,
                   "xOverlap measured against the narrower box");
        }

        // Chain: A-B and B-C are close, A-C are 30 px apart. Order A, C, B so the
        // first pair checked (A, C) does not unite; only union-find links A to C.
        {
            Blob a = boxBlob(100, 0, 50, 20);   // y 0..20
            Blob b = boxBlob(100, 25, 50, 20);  // y 25..45 (gap 5 to A and to C)
            Blob c = boxBlob(100, 50, 50, 20);  // y 50..70 (gap 30 to A)
            std::vector<Blob> out = groupFragments({a, c, b}, minXOverlap, maxGapY);
            expect(out.size() == 1, "chain A-B-C -> one blob");
            if (out.size() == 1) {
                expect(out[0].bounding_box == cv::Rect(100, 0, 50, 70), "chain: union box");
                expect(out[0].area == 3000, "chain: area sum");
                expect(out[0].centroid == cv::Point(125, 35), "chain: centroid");
            }
        }

        // Two separate groups plus a loner; output in order of each group's first member
        {
            Blob g1a = boxBlob(0, 0, 40, 20), g1b = boxBlob(0, 25, 40, 20);        // group 1
            Blob loner = boxBlob(500, 0, 40, 20);                                // alone
            Blob g2a = boxBlob(200, 0, 40, 20), g2b = boxBlob(200, 28, 40, 20);  // group 2
            std::vector<Blob> out = groupFragments({g1a, loner, g2a, g1b, g2b}, minXOverlap, maxGapY);
            expect(out.size() == 3, "two groups + loner -> 3 blobs");
            if (out.size() == 3) {
                expect(out[0].bounding_box == cv::Rect(0, 0, 40, 45), "first output = group 1");
                expect(sameBlob(out[1], loner), "second output = loner, unchanged");
                expect(out[2].bounding_box == cv::Rect(200, 0, 40, 48), "third output = group 2");
            }
        }
    }

    // Random inputs: properties that must hold for any grouping
    //   - total area is preserved
    //   - every input box lies inside some output box
    //   - no more outputs than inputs
    //   - any two inputs that satisfy the rule end up in the same output box
    void randomCases() {
        std::mt19937 rng(187);
        std::uniform_int_distribution<int> pos(0, 300), size(5, 60), count(0, 20);
        for (int t = 0; t < 2000; t++) {
            int n = count(rng);
            std::vector<Blob> in;
            long long totalArea = 0;
            for (int i = 0; i < n; i++) {
                Blob b = boxBlob(pos(rng), pos(rng), size(rng), size(rng));
                totalArea += b.area;
                in.push_back(b);
            }
            std::vector<Blob> out = groupFragments(in, minXOverlap, maxGapY);

            long long outArea = 0;
            for (const Blob& b : out) outArea += b.area;
            expect(outArea == totalArea, "random: total area preserved");
            expect(out.size() <= in.size(), "random: no more outputs than inputs");

            // Index of the output box containing each input box
            std::vector<int> home(n, -1);
            for (int i = 0; i < n; i++) {
                for (size_t k = 0; k < out.size(); k++) {
                    if ((in[i].bounding_box & out[k].bounding_box) == in[i].bounding_box) {
                        home[i] = static_cast<int>(k);
                        break;
                    }
                }
                expect(home[i] != -1, "random: input box inside some output box");
            }

            // Pairs that satisfy the rule must share an output (checked by containment,
            // since output boxes can overlap each other)
            for (int i = 0; i < n; i++) {
                for (int j = i + 1; j < n; j++) {
                    const cv::Rect& a = in[i].bounding_box;
                    const cv::Rect& b = in[j].bounding_box;
                    int shared = std::max(0, std::min(a.x + a.width, b.x + b.width) - std::max(a.x, b.x));
                    double xo = static_cast<double>(shared) / std::min(a.width, b.width);
                    int gy = std::max(0, std::max(a.y, b.y) - std::min(a.y + a.height, b.y + b.height));
                    if (xo >= minXOverlap && gy <= maxGapY) {
                        bool together = false;
                        for (const Blob& o : out)
                            if ((a & o.bounding_box) == a && (b & o.bounding_box) == b) together = true;
                        expect(together, "random: rule-satisfying pair in one output");
                    }
                }
            }
        }
    }
}

int main() {
    handCases();
    randomCases();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
