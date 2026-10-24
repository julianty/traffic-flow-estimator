#include "meta.hpp"
#include <string>
#include <filesystem>
#include <opencv2/core.hpp>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {
    std::string baseName(const std::string& path) {
        return fs::path(path).filename().string();
    }

    cv::FileNode findClip(const cv::FileStorage& fs, const std::string& fileName) {
        std::string targetName = baseName(fileName);
        for (const auto& clipNode : fs["clips"]) {
            // Linear search for matching clip
            std::string fn = clipNode["file"];
            if (targetName == fn) {
                return clipNode;
            }
        }
        std::string error_message = "Clip " + targetName + " not found in meta.json";
        throw std::runtime_error(error_message);
    }

    cv::Point readPoint(const cv::FileNode& countLine) {
        // Verify each point has entries
        if (countLine.size() != 2) throw std::runtime_error("Count line invalid in meta.json");
        return cv::Point(countLine[0], countLine[1]);
    }
    CountLine readCountLine(const cv::FileNode& clipNode) {
        cv::FileNode countLine = clipNode["annotations"]["count_line"];

        // Verify clipnode contains two points
        if (countLine.size() != 2) throw std::runtime_error("Count line not defined in meta.json");

        cv::Point lineA = readPoint(countLine[0]);
        cv::Point lineB = readPoint(countLine[1]);

        return CountLine{lineA, lineB};
    }
}

ClipMeta loadClipMeta(const std::string& metaPath, const std::string& videoPath) {
    // Open file
    cv::FileStorage fs{metaPath, cv::FileStorage::READ};

    // Check if open
    if (!fs.isOpened()) {
        std::string error_message = "Could not open file " + metaPath;
        throw std::runtime_error(error_message);
    }

    // Get clipNode
    cv::FileNode clipNode = findClip(fs, videoPath);

    // Read properties
    int width = clipNode["width"];
    int height = clipNode["height"];

    return ClipMeta{
        {readCountLine(clipNode)},
        width,
        height,
    };
}