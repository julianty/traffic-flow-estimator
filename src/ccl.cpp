#include "ccl.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <queue>
#include <cassert>

namespace {
    const cv::Point kNeighbors4[] = { {0,-1}, {-1,0}, {1,0}, {0,1} };
    const cv::Point kNeighbors8[] = { {-1,-1}, {0,-1}, {1,-1}, 
                                        {-1,0}, {1,0},
                                        {-1,1}, {0,1}, {1,1} };

    Blob floodFill(const cv::Mat& mask, cv::Mat& labels, const cv::Point& seed, 
                    int labelID, std::queue<cv::Point>& queue) {
        // Label seed
        labels.at<int>(seed.y, seed.x) = labelID;
        queue.push(cv::Point(seed.x, seed.y));

        // Create the blob tracking stats
        int minX = seed.x, maxX = seed.x;
        int minY = seed.y, maxY = seed.y;
        int count(0);
        int sumX = 0, sumY = 0;

        // Process the queue
        while (!queue.empty())  {
            cv::Point pt = queue.front();
            queue.pop();
            
            for (const auto& neighbor : kNeighbors4) {
                cv::Point newPt = cv::Point(neighbor.x + pt.x, neighbor.y + pt.y);
                // Check out of bounds
                if (newPt.x == -1 || newPt.x == mask.cols) continue;
                if (newPt.y == -1 || newPt.y == mask.rows) continue;

                if (mask.at<uchar>(newPt) != 0 && labels.at<int>(newPt) == 0) {
                    // This label can inherit the last label
                    labels.at<int>(newPt) = labelID;
                    // Enqueue its neighbors
                    queue.push(newPt);
                }
            }
            // Update blob stats
            if (pt.x < minX) minX = pt.x;
            if (pt.x > maxX) maxX = pt.x;
            if (pt.y < minY) minY = pt.y;
            if (pt.y > maxY) maxY = pt.y;
            sumX += pt.x;
            sumY += pt.y;
            count++;
            
        }
        // Compute blob
        Blob blob = {cv::Rect(minX, minY, maxX - minX + 1, maxY - minY + 1), 
            count, 
            cv::Point(sumX / count, sumY / count)};
        return blob;
    }
} // namespace
void label(const cv::Mat& mask, cv::Mat& labels, std::vector<Blob>& blobs) {
    // Create image
    labels.create(mask.rows, mask.cols, CV_32SC1);
    // Ensure zeroing across iterations
    labels.setTo(0);
    // Empty blobs
    blobs.clear();
    
    int nextLabel(1);
    std::queue<cv::Point> processQueue;
    for (int y=0; y < mask.rows; y++) {
        for (int x=0; x < mask.cols; x++) {
            if (mask.at<uchar>(y, x) != 0 && labels.at<int>(y, x) == 0) {
                Blob newBlob = floodFill(mask, labels, cv::Point(x,y), nextLabel, processQueue);
                blobs.push_back(newBlob);
                nextLabel++;
            }
        }
    }

    #ifndef NDEBUG
        // Checksum
        int total = 0;
        for (const Blob& blob : blobs) total += blob.area;
        assert(total == cv::countNonZero(mask) && "blob areas don't match foreground pixels");
    #endif

}