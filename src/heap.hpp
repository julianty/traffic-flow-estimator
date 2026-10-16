#pragma once

#include <vector>
#include "ccl.hpp"

// Blobs with equal area will be further sorted by their y first, smaller wins, 
// and then x pos, smaller wins.
// Invariant: parent >= children, children at 2i+1 and 2i+2
class BlobMaxHeap {
    private:
        std::vector<Blob> data;
        void siftDown(int idx);
    public:
        BlobMaxHeap(const std::vector<Blob>& blobs);
        Blob top() const;
        void pop();
        bool empty() const;
        size_t size() const;
};