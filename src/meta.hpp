#pragma once
#include <opencv2/core.hpp>
#include <string>

struct CountLine {
    cv::Point countLineA;
    cv::Point countLineB;
};

struct ClipMeta {
    CountLine countLine;
    int width;
    int height;
};

// Throws std::runtime_error if
//      file can't be opened,
//      no clip matches
//      no count_line is set
//      the count_line is malformed
ClipMeta loadClipMeta(const std::string& metaPath, const std::string& videoPath);