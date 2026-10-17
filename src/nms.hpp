#pragma once
#include "ccl.hpp"
#include <vector>
// Takes vector of blobs from the labels function, a IoU threshold, 
// and outputs a vector of blobs that pass the threshold
std::vector<Blob> nms(const std::vector<Blob>& blobs, double iouThreshold);