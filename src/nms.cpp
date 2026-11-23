#include "nms.hpp"
#include "ccl.hpp"
#include <opencv2/opencv.hpp>
#include "heap.hpp"
#include <algorithm>

namespace {
    // Takes two cv::Rect, and outputs the IoU score
    double iouScore(const cv::Rect& rect1, const cv::Rect& rect2) {
        double score = 0;
        // Extract opposing corner points top-left and bottom-right
        int x1 = rect1.x; int y1 = rect1.y;
        cv::Point br1 = rect1.br();
        int a1 = br1.x; int b1 = br1.y;
        
        int x2 = rect2.x; int y2 = rect2.y;
        cv::Point br2 = rect2.br();
        int a2 = br2.x; int b2 = br2.y;
        
        // First check if they intersect
        if (x1 > a2 || x2 > a1) return 0;
        if (y1 > b2 || y2 > b1) return 0;
        
        // Find top left of intersection
        int x_int = x1 < x2 ? x2 : x1;
        int y_int = y1 < y2 ? y2 : y1;
        
        // Find bottom right of intersection
        int a_int = a1 < a2 ? a1 : a2;
        int b_int = b1 < b2 ? b1 : b2;

        // Compute the intersection area
        int intersectionArea = (a_int - x_int) * (b_int - y_int);

        // Compute the union area
        int unionArea = ((a1 - x1) * (b1 - y1)) + ((a2 - x2) * (b2 - y2)) - intersectionArea;

        // Compute the score
        if (unionArea <= 0) return 0;
        score = static_cast<double>(intersectionArea) / unionArea;
        return score;
    }
    // Takes two blobs, and returns true if the first is prioritized, and false otherwise
    bool prefer(const Blob& b1, const Blob& b2) {
        // prioritize size
        if (b1.area > b2.area) {
            return true;
        } else if (b2.area > b1.area) {
            return false;
        }
        // prioritize smaller y
        if (b1.bounding_box.y < b2.bounding_box.y) {
            return true;
        } else if (b1.bounding_box.y > b2.bounding_box.y) {
            return false;
        }
        // prioritize smaller x
        if (b1.bounding_box.x < b2.bounding_box.x) {
            return true;
        } else if (b1.bounding_box.x > b2.bounding_box.x) {
            return false;
        }
        // b2 wins by default
        return false;
    }
}


std::vector<Blob> nms(const std::vector<Blob>& blobs, double iouThreshold) {
    // Initialize heap
    BlobMaxHeap blobHeap(blobs);
    std::vector<Blob> keep;

    // Main loop for adding candidates
    while (!blobHeap.empty()) {
        Blob candidate = blobHeap.top();
        bool disqualify = false;
        blobHeap.pop();

        // Check against all other kept blobs
        for (const auto& kBlob : keep) {
            double score = iouScore(kBlob.bounding_box, candidate.bounding_box);
            if (score > iouThreshold) {
                disqualify = true;
                // Early stop if disqualified
                break;
            }
        }
        if (!disqualify) keep.push_back(candidate);
    }
    return keep;
}


std::vector<Blob> brute_force_nms(const std::vector<Blob>& blobs, double iouThreshold) {
    // Create a copy
    std::vector<Blob> blobs_copy = blobs;
    // Sort by rank order
    std::sort(blobs_copy.begin(), blobs_copy.end(), [](const Blob& b1, const Blob& b2) {return prefer(b1, b2);});

    // Initialize keep
    std::vector<Blob> keep{};

    // Compare each blob in rank order against the keep vector
    for (size_t i=0; i<blobs_copy.size(); i++) {
        const Blob& current = blobs_copy[i];

        // Check against kept blobs
        bool disqualify = false;
        for (size_t j=0; j<keep.size(); j++) {
            const Blob& other = keep[j];
            double score = iouScore(current.bounding_box, other.bounding_box);
            if (score > iouThreshold) {
                disqualify = true;
                break;
            }
        }
        // Save it if it wasn't disqualified
        if (!disqualify) keep.push_back(current);
    }
    return keep;
}
