// Renders one frame's foreground mask under several MOG2 parameter sets, side by side.
// Every variant sees the same frames from the start of the clip (MOG2 needs its history),
// then the target frame goes through the same threshold -> open -> close -> label steps
// as main.cpp. Edit the `variants` table below to try other settings.
//
// Build: make build/mog2_tune.exe
// Run from the repo root:
//   build/mog2_tune.exe <video_path> [frame=336] [scale=0.5] [out=build/mog2_tune.png]
// `frame` counts from 1, like the frame counter in main.
#include "../src/ccl.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>

namespace {
    struct Variant {
        std::string name;
        double varThreshold;
        int history;
        int blurK;           // Gaussian blur kernel on the input (odd); 0 or 1 = no blur
        double learningRate; // -1 = MOG2's automatic rate
        int warmupFrames;    // frames 1..warmupFrames use the automatic rate, then learningRate
        int openK;           // morphological open kernel (square); 0 or 1 = skip
        int closeK;          // morphological close kernel (square); 0 or 1 = skip
        int neighbors;       // blob connectivity: 4 or 8
    };

    // Edit freely. The first entry should be your current settings in main.cpp.
    const std::vector<Variant> variants = {
        // name                    vt    hist  blur  lr      warmup open close nb
        {"4-conn, close 7",        16.0,  500, 0,  0.003,  0,     3,   7,    4},
        {"8-conn, close 7",        16.0,  500, 0,  0.003,  0,     3,   7,    8},
        {"4-conn, close 3",        16.0,  500, 0,  0.003,  0,     3,   3,    4},
        {"8-conn, close 3",        16.0,  500, 0,  0.003,  0,     3,   3,    8},
        {"4-conn, no close",       16.0,  500, 0,  0.003,  0,     3,   0,    4},
        {"8-conn, no close",       16.0,  500, 0,  0.003,  0,     3,   0,    8},
    };

    constexpr bool detectShadows = true;
    constexpr int fgThreshold = 200;      // same as main
    constexpr int blobAreaThreshold = 100; // same as main (at scale 0.5)
    constexpr int columns = 3;
    constexpr int refFrameHeight = 540;

    // Threshold, morphology and labeling, mirroring main.cpp. Returns the blobs.
    std::vector<Blob> maskToBlobs(const cv::Mat& rawMask, cv::Mat& filterMask, int openK, int closeK,
                                  int neighbors) {
        cv::Mat binMask;
        cv::threshold(rawMask, binMask, fgThreshold, 255, cv::THRESH_BINARY);
        filterMask = binMask.clone();
        if (openK > 1) {
            cv::Mat k = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(openK, openK));
            cv::morphologyEx(filterMask, filterMask, cv::MORPH_OPEN, k);
        }
        if (closeK > 1) {
            cv::Mat k = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(closeK, closeK));
            cv::morphologyEx(filterMask, filterMask, cv::MORPH_CLOSE, k);
        }
        cv::Mat labels;
        std::vector<Blob> blobs;
        label(filterMask, labels, blobs, neighbors == 8 ? Neighbors::Eight : Neighbors::Four);
        return blobs;
    }

    void caption(cv::Mat& tile, const std::string& line1, const std::string& line2) {
        double ratio = static_cast<double>(tile.rows) / refFrameHeight;
        double fontScale = 0.8 * ratio;
        int wt = std::max(1, static_cast<int>(std::lround(2 * ratio)));
        int pad = static_cast<int>(std::lround(10 * ratio));
        int baseline = 0;
        cv::Size sz = cv::getTextSize(line1, cv::FONT_HERSHEY_SIMPLEX, fontScale, wt, &baseline);
        int y1 = pad + sz.height;
        int y2 = y1 + sz.height + baseline + pad / 2;
        cv::putText(tile, line1, {pad, y1}, cv::FONT_HERSHEY_SIMPLEX, fontScale, {0, 255, 0}, wt);
        cv::putText(tile, line2, {pad, y2}, cv::FONT_HERSHEY_SIMPLEX, fontScale, {0, 255, 0}, wt);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <video_path> [frame=336] [scale=0.5] [out=build/mog2_tune.png]\n";
        return 1;
    }
    std::string videoPath = argv[1];
    int targetFrame = 336;
    double scale = 0.5;
    std::string outPath = "build/mog2_tune.png";
    try {
        if (argc > 2) targetFrame = std::stoi(argv[2]);
        if (argc > 3) scale = std::stod(argv[3]);
    } catch (const std::exception&) {
        std::cerr << "frame must be an integer and scale a number\n";
        return 1;
    }
    if (argc > 4) outPath = argv[4];
    if (targetFrame < 1 || scale <= 0.0 || scale > 1.0) {
        std::cerr << "need frame >= 1 and 0 < scale <= 1\n";
        return 1;
    }

    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Could not open " << videoPath << "\n";
        return 1;
    }

    std::vector<cv::Ptr<cv::BackgroundSubtractorMOG2>> subtractors;
    for (const Variant& v : variants) {
        subtractors.push_back(cv::createBackgroundSubtractorMOG2(v.history, v.varThreshold, detectShadows));
    }
    std::vector<cv::Mat> masks(variants.size());

    cv::Mat img, imgScaled, input;
    int frameCount = 0;
    while (frameCount < targetFrame && cap.read(img)) {
        frameCount++;
        cv::resize(img, imgScaled, cv::Size(), scale, scale, cv::INTER_AREA);
        for (size_t i = 0; i < variants.size(); i++) {
            if (variants[i].blurK > 1) {
                cv::GaussianBlur(imgScaled, input, cv::Size(variants[i].blurK, variants[i].blurK), 0);
            } else {
                input = imgScaled;
            }
            double lr = frameCount <= variants[i].warmupFrames ? -1.0 : variants[i].learningRate;
            subtractors[i]->apply(input, masks[i], lr);
        }
    }
    if (frameCount < targetFrame) {
        std::cerr << "Video ended at frame " << frameCount << ", before frame " << targetFrame << "\n";
        return 1;
    }

    // Tile 0 is the frame itself; the rest are one variant each, with blob boxes drawn
    std::vector<cv::Mat> tiles;
    cv::Mat original = imgScaled.clone();
    caption(original, "frame " + std::to_string(targetFrame), "scale " + std::to_string(scale).substr(0, 4));
    tiles.push_back(original);

    std::printf("frame %d, scale %.2f, area filter >= %d\n", targetFrame, scale, blobAreaThreshold);
    for (size_t i = 0; i < variants.size(); i++) {
        cv::Mat filterMask;
        std::vector<Blob> blobs = maskToBlobs(masks[i], filterMask, variants[i].openK, variants[i].closeK,
                                             variants[i].neighbors);
        int total = static_cast<int>(blobs.size());
        int kept = 0;
        cv::Mat tile;
        cv::cvtColor(filterMask, tile, cv::COLOR_GRAY2BGR);
        for (const Blob& b : blobs) {
            if (b.area >= blobAreaThreshold) {
                kept++;
                cv::rectangle(tile, b.bounding_box, cv::Scalar(225, 0, 0), 2);
            }
        }
        int fgPixels = cv::countNonZero(filterMask);
        caption(tile, variants[i].name, "blobs " + std::to_string(kept) + " / " + std::to_string(total));
        tiles.push_back(tile);
        std::printf("%-22s blobs>=%d: %3d  all: %3d  fg pixels: %d\n",
                    variants[i].name.c_str(), blobAreaThreshold, kept, total, fgPixels);
    }

    // Pad to a full last row, then stack rows
    while (tiles.size() % columns != 0) {
        tiles.push_back(cv::Mat::zeros(tiles[0].size(), tiles[0].type()));
    }
    std::vector<cv::Mat> rows;
    for (size_t i = 0; i < tiles.size(); i += columns) {
        cv::Mat row;
        cv::hconcat(std::vector<cv::Mat>(tiles.begin() + i, tiles.begin() + i + columns), row);
        rows.push_back(row);
    }
    cv::Mat grid;
    cv::vconcat(rows, grid);

    if (!cv::imwrite(outPath, grid)) {
        std::cerr << "Could not write " << outPath << "\n";
        return 1;
    }
    std::cout << "Wrote " << outPath << " (" << grid.cols << "x" << grid.rows << ")\n";
    return 0;
}
