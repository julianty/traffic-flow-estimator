#include "heap.hpp"
#include <cassert>
#include <algorithm>

namespace {
    // rank helper
    bool aLTb(const Blob& a, const Blob& b) {
        // Compare areas
        if (a.area != b.area) {
            return a.area < b.area;
        }
        // Compare y coord
        if (a.bounding_box.y != b.bounding_box.y) {
            return a.bounding_box.y > b.bounding_box.y;
        }
        // Compare x coord
        if (a.bounding_box.x != b.bounding_box.x) {
            return a.bounding_box.x > b.bounding_box.x;
        }
        // otherwise, just return false
        return false;
    }
}


void BlobMaxHeap::siftDown(int idx) {
    int n = static_cast<int>(data.size());
    int left = 2*idx + 1;
    int right = 2*idx + 2;
    if (left >= n) return;
    // Compare children
    int bestChildIdx = left;
    // Check if right exists
    if (right < n) {
        bestChildIdx = aLTb(data[left], data[right]) ? right : left;
    }
    if (aLTb(data[idx], data[bestChildIdx])) {
        std::swap(data[bestChildIdx], data[idx]);
        siftDown(bestChildIdx);
    }
}

BlobMaxHeap::BlobMaxHeap(const std::vector<Blob>& blobs) : data(blobs) {
    int n = static_cast<int>(data.size());
    for (int i=n/2 - 1; i >= 0; i--) {
        siftDown(i);
    }
}

Blob BlobMaxHeap::top() const {
    assert(!empty());
    return data[0];
}

void BlobMaxHeap::pop() {
    assert(!empty());

    // swap the first and last element
    std::swap(data[0], data[size()-1]);

    // remove the last element
    data.pop_back();
    
    // siftDown the last element
    siftDown(0);
}

bool BlobMaxHeap::empty() const {
    return size() == 0;
}
size_t BlobMaxHeap::size() const {
    return data.size();
}