#pragma once

#include <opencv2/core.hpp>
#include "meta.hpp"
#include "ccl.hpp"
#include <vector>

struct Track {
    int id;
    cv::Point2f centroid;
    cv::Point2f prevCentroid;
    int missed = 0;
    int hits = 1;
    bool counted = false;
};

class Tracker {
public:
    Tracker(CountLine line);
    void update(const std::vector<Blob>& blobs, bool countingEnabled);
    const std::vector<Track>& tracks() const;
    int count() const;
private:
    int count_ = 0;
    CountLine line_;
    std::vector<Track> tracks_;
    int nextId_ = 0;
};