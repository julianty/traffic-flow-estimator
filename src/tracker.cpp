#include "tracker.hpp"

#include <opencv2/core.hpp>
#include "meta.hpp"
#include "ccl.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
namespace {
    constexpr int maxMatchDistance = 100; // tracks will match with blobs if their centroid last frame is with 100 pixels
    constexpr int maxMissed = 5; // tracks will be dropped when in 5 frames, no blob is matched
    constexpr int minHits = 3; // tracks will not be counted until 3 blob matches were made

    struct Candidate {
        size_t track;
        size_t blob;
        double dist;
    };

    // Compute cross product of A-B x A-P
    double side(const CountLine& line, cv::Point2f p) {
        cv::Point2f aMinusB = line.countLineA - line.countLineB;
        cv::Point2f aMinusP = cv::Point2f(line.countLineA) - p;
        return aMinusB.cross(aMinusP);
    }

    double distanceBetween(cv::Point2f a, cv::Point2f b) {
        cv::Point2f displacement = a - b;
        return std::sqrt(displacement.dot(displacement));
    }
} // namespace

Tracker::Tracker(CountLine line) {line_ = line;}

int Tracker::count() const {
    return count_;
}
const std::vector<Track>& Tracker::tracks() const {
    return tracks_;
}

void Tracker::update(const std::vector<Blob>& blobs, bool countingEnabled) {
    // Create a container for the candidates
    std::vector<Candidate> candidates;

    // Loop over each blob
    for (size_t i=0; i < blobs.size(); i++) {
        cv::Point2f a = blobs[i].centroid;
        for (size_t j=0; j < tracks_.size(); j++) {
            // Compute the distance of each blob to each track
            cv::Point2f b = tracks_[j].centroid;
            double dist = distanceBetween(a, b);

            // Save the point if it's within maxdistance
            if (dist < maxMatchDistance) candidates.push_back(Candidate{j, i, dist});
            
        }
    }

    // Sort candidates by distance descending
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a,const Candidate& b){
        return a.dist > b.dist;
    });

    // Create a flag to mark used tracks
    std::vector<bool> trackUsed(tracks_.size(), false);
    // Create a flag to mark used blobs
    std::vector<bool> blobUsed(blobs.size(), false);

    // Greedily save matches
    while (candidates.size() > 0) {
        // Take lowest distance candidate
        Candidate back = candidates.back();
        candidates.pop_back();

        // Check each candidate against its track
        if (!trackUsed[back.track] && !blobUsed[back.blob]) {
            // Track has not been claimed
            Track& t = tracks_[back.track];
            t.prevCentroid = t.centroid;
            t.centroid = blobs[back.blob].centroid;
            trackUsed[back.track] = true;
            blobUsed[back.blob] = true;
        }
    }

    // Update tracks based on use
    for (size_t i=0; i < tracks_.size(); i++) {
        if (trackUsed[i]) {
            tracks_[i].hits += 1;
            tracks_[i].missed = 0;
        } else {
            tracks_[i].missed++;
        }
    }
    // Count the crossings
    // Check if counting is enabled
    if (countingEnabled) {
        // Loop through each track
        for (auto& t: tracks_) {
            if (t.counted == false && 
                    t.hits >= minHits &&
                    (side(line_, t.prevCentroid) >= 0) != (side(line_, t.centroid) >= 0) ) {
                // Increment count if the track wasn't counted and if the centroid swapped sides 
                // and if the number of hits passes the minimum
                count_ += 1;
                t.counted = true;                
            }
        }
    }

    // Add new tracks for blobs that were not matched
    for (size_t i=0; i < blobs.size(); i++) {
        if (!blobUsed[i]) {
            tracks_.push_back({nextId_, blobs[i].centroid, blobs[i].centroid});
            nextId_++;
        }
    }

    // Delete dead tracks
    auto removeIter = std::remove_if(tracks_.begin(), tracks_.end(), [](const Track& t){
        return t.missed >= maxMissed;
    });
    tracks_.erase(removeIter, tracks_.end());


}