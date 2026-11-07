#include "group.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include "ccl.hpp"
#include <opencv2/core.hpp>
#include "unionfind.hpp"

namespace {
    bool sameVehicle(const cv::Rect& r1, const cv::Rect& r2, double minXOverlap, int maxGapY) {
        cv::Range ax(r1.x, r1.x + r1.width);
        cv::Range bx(r2.x, r2.x + r2.width);
        double xOverlap = static_cast<double>((ax & bx).size()) / std::min(r1.width, r2.width);   // 0 if disjoint
        int gapY = std::max(0, std::max(r1.y, r2.y) - std::min(r1.br().y, r2.br().y));
        return xOverlap >= minXOverlap && gapY <= maxGapY;
    }
};

std::vector<Blob> groupFragments(const std::vector<Blob>& blobs, double minXOverlap, int maxGapY) {
    int n = blobs.size();
    UnionFind uf(n);

    for (int i=0; i < n; i++) {
        for (int j = i+1; j < n; j++)
        {
            if (sameVehicle(blobs[i].bounding_box, blobs[j].bounding_box, minXOverlap, maxGapY)) {
                uf.unite(i, j);
            }
        }
    }

    std::vector<int> slot(n, -1);
    std::vector<Blob> out;
    // Helpers for calculating group centroids. Updated centeroid is a weighted sum of group centroids
    std::vector<double> sumX;
    std::vector<double> sumY;
    for (int i=0; i<n; i++) {
        int r = uf.find(i);
        if (slot[r] == -1) {
            // create a new group
            slot[r] = out.size();
            out.push_back(blobs[i]);
            // add centroid information
            sumX.push_back(blobs[i].area * blobs[i].centroid.x);
            sumY.push_back(blobs[i].area * blobs[i].centroid.y);
        } else {
            // the group is already represented in blobs
            Blob& root = out[slot[r]];
            root.bounding_box = root.bounding_box | blobs[i].bounding_box;
            root.area = root.area + blobs[i].area;
            // calculate centroid sum contribution
            sumX[slot[r]] += blobs[i].area * blobs[i].centroid.x;
            sumY[slot[r]] += blobs[i].area * blobs[i].centroid.y;
        }
    }
    for (size_t i=0; i<out.size(); i++) {
        // Calculate each centroid in out after groups are done
        int cx = round(sumX[i] / out[i].area);
        int cy = round(sumY[i] / out[i].area);
        out[i].centroid = cv::Point(cx, cy);
    }

    return out;
}