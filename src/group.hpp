#pragma once
#include <vector>
#include "ccl.hpp"

std::vector<Blob> groupFragments(const std::vector<Blob>& blobs, double minXOverlap, int maxGapY);