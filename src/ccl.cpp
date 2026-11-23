#include "ccl.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <queue>
#include <cassert>
#include <stdexcept>
#include <map>
#include <unordered_map>
namespace {
    const cv::Point kNeighbors4[] = { {0,-1}, {-1,0}, {1,0}, {0,1} };
    const cv::Point kNeighbors8[] = { {-1,-1}, {0,-1}, {1,-1}, 
                                        {-1,0}, {1,0},
                                        {-1,1}, {0,1}, {1,1} };

    Blob floodFill(const cv::Mat& mask, cv::Mat& labels, const cv::Point& seed, 
                    int labelID, std::queue<cv::Point>& queue, int kNeighbors) {
        // Set kernel
        const cv::Point* offsets = kNeighbors == 4 ? kNeighbors4 : kNeighbors8;

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

            for (int i=0; i<kNeighbors; i++) {
                cv::Point neighbor = offsets[i];
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
    struct blob_calc 
    {
        int count;
        int sumX;
        int sumY;
        int minX;
        int maxX;
        int minY;
        int maxY;
    };
} // namespace
void label(const cv::Mat& mask, cv::Mat& labels, std::vector<Blob>& blobs, Neighbors kNeighbors) {
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
                Blob newBlob = floodFill(mask, labels, cv::Point(x,y), nextLabel, processQueue, static_cast<int>(kNeighbors));
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

void brute_force_label(const cv::Mat& mask, cv::Mat& labels, std::vector<Blob>& blobs, Neighbors kNeighbors) {
    // Create image
    labels.create(mask.rows, mask.cols, CV_32SC1);
    // Copy mask into labels
    mask.convertTo(labels, CV_32S);
    // Initialize labels
    for (int i=0; i<labels.rows; i++) {
        for (int j=0; j<labels.cols; j++) {
            int current_label = labels.at<int>(i, j);
            if (current_label == 0) continue;
            labels.at<int>(i, j) = i * labels.cols + j + 1;
        }
    }

    // Empty blobs
    blobs.clear();

    // Extract neighbor count
    int neighbor_count = static_cast<int>(kNeighbors);

    // Create stop flag
    bool stop = false;
    // Set kernel
    const cv::Point* offsets = kNeighbors == Neighbors::Four ? kNeighbors4 : kNeighbors8;
    while (!stop) {
        // If this flag isn't re-set to false, then no changes were made
        stop = true;
        // Do a sweep of labels
        for (int i=0; i < labels.rows; i++) {
            for (int j=0; j < labels.cols; j++) {
                int current_label = labels.at<int>(i, j);
                if (current_label == 0) {
                    // Skip if a background pixel
                    continue;
                }
                for (int k=0; k< neighbor_count; k++) {
                    cv::Point neighbor = offsets[k];
                    cv::Point newPt = cv::Point(neighbor.x + j, neighbor.y + i);
                    // Check out of bounds
                    if (newPt.x == -1 || newPt.x == mask.cols) continue;
                    if (newPt.y == -1 || newPt.y == mask.rows) continue;

                    // Check that the neighbor label is non-zero
                    int neighbor_label = labels.at<int>(newPt);
                    if (neighbor_label == 0) continue;
                    // Check if the neighbor has a smaller label than the current point
                    if (neighbor_label < current_label) {
                        // The current label should inherit the neighbor label
                        current_label = neighbor_label;
                        labels.at<int>(i, j) = neighbor_label;
                        stop = false;
                    }
                }
            }
        }
    }

    // Blob save pass
    std::map<int, blob_calc> blob_calcs;
    for (int i=0; i<labels.rows; i++) {
        for (int j=0; j<labels.cols; j++) {
            int current_label = labels.at<int>(i, j);
            if (current_label == 0) continue;
            // Check if label exists in map
            if (blob_calcs.count(current_label)) {
                // label exists, update the numbers
                blob_calc& record = blob_calcs[current_label];
                record.count += 1;
                record.sumX += j;
                record.sumY += i;
                record.minX = record.minX < j ? record.minX : j;
                record.maxX = record.maxX > j ? record.maxX : j;
                record.minY = record.minY < i ? record.minY : i;
                record.maxY = record.maxY > i ? record.maxY : i;
            } else {
                // create a new record
                blob_calc new_calc{};
                new_calc.count = 1;
                new_calc.sumX = j;
                new_calc.sumY = i;
                new_calc.minX = j;
                new_calc.maxX = j;
                new_calc.minY = i;
                new_calc.maxY = i;
                blob_calcs[current_label] = new_calc;
            }
        }
    }
    // Relabel labels
    int nextLabel = 1;
    std::unordered_map<int, int> old_new_labels;
    for (int i=0; i<labels.rows; i++) {
        for (int j=0; j<labels.cols; j++) {
            int current_label = labels.at<int>(i, j);
            if (current_label == 0) continue;
            // Check if the label already has a mapping
            if (old_new_labels.count(current_label)) {
                // use the established mapping
                labels.at<int>(i, j) = old_new_labels[current_label];
            } else {
                // create a new mapping and set the pixel
                old_new_labels[current_label] = nextLabel;
                labels.at<int>(i, j) = nextLabel++;
            }
        }
    }
    // build final blobs vector
    for (const auto& [k, calc] : blob_calcs) {
        Blob new_blob{};
        new_blob.area = calc.count;
        new_blob.bounding_box = cv::Rect(cv::Point(calc.minX, calc.minY), 
            cv::Point(calc.maxX + 1, calc.maxY + 1));
        new_blob.centroid = cv::Point(calc.sumX / calc.count, calc.sumY / calc.count);
        blobs.push_back(new_blob);
    }
}