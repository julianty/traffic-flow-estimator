// Records the pipeline overlay (frame | mask, boxes, tracks, crossing count) to an .mp4 for demos.
// Usage: make_demo <video> [out=build/demo.mp4] [scale=0.5] [start=121] [end=400] [group=1]
// group=0 skips the union-find fragment grouping stage (shows the pipeline before it was added).
// The whole clip up to `end` is processed so MOG2 and the tracker have their history;
// only frames start..end are written. Pipeline constants are a manual copy of main.cpp's.
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include "../src/ccl.hpp"
#include "../src/group.hpp"
#include "../src/meta.hpp"
#include "../src/nms.hpp"
#include "../src/tracker.hpp"

namespace {
    constexpr int warmupFrames = 120;
    constexpr int mog2History = 500;
    constexpr double mog2VarThreshold = 16;
    constexpr double mog2LearningRate = 0.003;
    constexpr int blobAreaThreshold = 200;
    constexpr double iouThreshold = 0.5;
    constexpr double minXOverlap = 0.5;
    constexpr int maxGapY = 10; // processing-scale pixels, sized at scale 0.5
    constexpr int openKernelSize = 3;
    constexpr int closeKernelSize = 7;
    const std::string metadataPath = "source-videos/meta.json";

    void draw(const cv::Mat& imgScaled, const cv::Mat& mask, int frameCount, const std::vector<Blob>& blobs,
              cv::Point lineA, cv::Point lineB, const Tracker& tracker, cv::Mat& frame) {
        cv::Mat mask3c;
        cv::cvtColor(mask, mask3c, cv::COLOR_GRAY2BGR);
        cv::hconcat(imgScaled, mask3c, frame);

        cv::line(frame, lineA, lineB, {0, 0, 255}, 2);
        cv::line(frame, lineA + cv::Point(imgScaled.cols, 0), lineB + cv::Point(imgScaled.cols, 0), {0, 0, 255}, 2);

        for (const Blob& b : blobs) cv::rectangle(frame, b.bounding_box, cv::Scalar(225, 0, 0), 2);

        double ratio = static_cast<double>(frame.rows) / 540;
        double fontScale = 1.2 * ratio;
        int fontWt = std::max(1, static_cast<int>(std::lround(2 * ratio)));
        int margin = static_cast<int>(std::lround(40 * ratio));

        for (const Track& t : tracker.tracks()) {
            cv::Point pt(t.centroid);
            cv::Scalar color = t.counted ? cv::Scalar(0, 255, 0) : cv::Scalar(128, 128, 0);
            cv::circle(frame, pt, 4, color, -1);
            cv::putText(frame, std::to_string(t.id), pt + cv::Point(2, -2),
                        cv::FONT_HERSHEY_SIMPLEX, fontScale * 0.4, color, 1);
        }

        int baseline = 0;
        cv::Size textSize = cv::getTextSize("Frame: 0000", cv::FONT_HERSHEY_SIMPLEX, fontScale, fontWt, &baseline);
        cv::Point origin(frame.cols - textSize.width - margin, frame.rows - margin - baseline);
        cv::putText(frame, "Frame: " + std::to_string(frameCount), origin,
                    cv::FONT_HERSHEY_SIMPLEX, fontScale, {0, 255, 0}, fontWt);
        cv::putText(frame, "Crossings: " + std::to_string(tracker.count()),
                    origin - cv::Point(0, textSize.height + baseline),
                    cv::FONT_HERSHEY_SIMPLEX, fontScale, {0, 255, 0}, fontWt);
        cv::putText(frame, "Blobs: " + std::to_string(blobs.size()),
                    origin - cv::Point(0, 2 * (textSize.height + baseline)),
                    cv::FONT_HERSHEY_SIMPLEX, fontScale, {0, 255, 0}, fontWt);
    }
} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <video> [out=build/demo.mp4] [scale=0.5] [start=121] [end=400] [group=1]\n";
        return 1;
    }
    std::string videoPath = argv[1];
    std::string outPath = argc > 2 ? argv[2] : "build/demo.mp4";
    double scale = argc > 3 ? std::stod(argv[3]) : 0.5;
    int startFrame = argc > 4 ? std::stoi(argv[4]) : warmupFrames + 1;
    int endFrame = argc > 5 ? std::stoi(argv[5]) : 400;
    bool useGrouping = argc > 6 ? std::stoi(argv[6]) != 0 : true;

    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) { std::cerr << "Can't open " << videoPath << "\n"; return 1; }
    double fps = cap.get(cv::CAP_PROP_FPS);

    ClipMeta meta;
    try {
        meta = loadClipMeta(metadataPath, videoPath);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    cv::Point lineA = meta.countLine.countLineA * scale;
    cv::Point lineB = meta.countLine.countLineB * scale;

    auto mog2 = cv::createBackgroundSubtractorMOG2(mog2History, mog2VarThreshold, true);
    cv::Mat openKernel = cv::getStructuringElement(cv::MORPH_RECT, {openKernelSize, openKernelSize});
    cv::Mat closeKernel = cv::getStructuringElement(cv::MORPH_RECT, {closeKernelSize, closeKernelSize});
    Tracker tracker{CountLine{lineA, lineB}};

    cv::VideoWriter writer;
    cv::Mat img, imgScaled, mask, binMask, filterMask, labels, frame;
    std::vector<Blob> blobs;
    int frameCount = 0;

    while (frameCount < endFrame && cap.read(img)) {
        cv::resize(img, imgScaled, cv::Size(), scale, scale, cv::INTER_AREA);
        mog2->apply(imgScaled, mask, mog2LearningRate);
        cv::threshold(mask, binMask, 200, 255, cv::THRESH_BINARY);
        cv::morphologyEx(binMask, filterMask, cv::MORPH_OPEN, openKernel);
        cv::morphologyEx(filterMask, filterMask, cv::MORPH_CLOSE, closeKernel);
        label(filterMask, labels, blobs, Neighbors::Four);
        frameCount++;

        blobs.erase(std::remove_if(blobs.begin(), blobs.end(),
                                   [](const Blob& b) { return b.area < blobAreaThreshold; }),
                    blobs.end());
        std::vector<Blob> kept = nms(blobs, iouThreshold);
        if (useGrouping) kept = groupFragments(kept, minXOverlap, maxGapY);
        tracker.update(kept, frameCount > warmupFrames);

        if (frameCount < startFrame) continue;
        draw(imgScaled, filterMask, frameCount, kept, lineA, lineB, tracker, frame);
        if (!writer.isOpened()) {
            writer.open(outPath, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, frame.size());
            if (!writer.isOpened()) { std::cerr << "Can't open writer for " << outPath << "\n"; return 1; }
        }
        writer.write(frame);
    }
    std::cout << "Wrote frames " << startFrame << "-" << frameCount << " to " << outPath
              << ", crossings at end: " << tracker.count() << "\n";
    return 0;
}
