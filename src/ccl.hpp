#pragma once

#include <opencv2/opencv.hpp>

struct Blob
{
    cv::Rect bounding_box;
    int area;
    cv::Point centroid;
};

// Expects a binary CV_8UC1 mask, and fills labels as CV_32SC1
void label(const cv::Mat& mask, cv::Mat& labels, std::vector<Blob>& blobs);
