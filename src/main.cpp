#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
#include <opencv2/imgproc.hpp>
#include "args.hpp"
#include "ccl.hpp"
#include "nms.hpp"
#include "meta.hpp"
#include "tracker.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <vector>
#include <queue>
#include <optional>


namespace {
    // Pipeline Parameters
    //    Mog2
    constexpr int warmupFrames = 120; // Ghosts appear before MOG2 learns the background pixels
    constexpr int mog2History = 500; // default 500
    constexpr double mog2VarThreshold = 16; // default 16.0
    constexpr double mog2LearningRate = 0.003; // default -1 (it gets computed as a fxn of history)
    //    blob filtering
    const int blobAreaThreshold = 200;
    constexpr double iouThreshold = 0.5;
    //    metadata path
    const std::string metadataPath = "source-videos/meta.json";


    // Parameter order (both functions): inputs, then outputs
    void process(const cv::Mat& img, const cv::Ptr<cv::BackgroundSubtractorMOG2>& mog2, double scale,
                 cv::Mat& imgScaled, cv::Mat& mask) {
        // Exact == is safe: 1.0 is exactly representable and comes straight from the default / stod
        if (scale == 1.0) {
            // Shallow copy: shares img's pixels, so never draw on imgScaled
            imgScaled = img;
        } else {
            // Resize image
            cv::resize(img, imgScaled, cv::Size(), scale, scale, cv::INTER_AREA);
        }

        // Run MOG2
        mog2->apply(imgScaled, mask, mog2LearningRate);
        
    }
    // Font control constants
    constexpr double refFontScale = 1.2;
    constexpr int refFontWt = 2;
    constexpr int refFrameHeight = 540;
    constexpr int refMargin = 40;
    constexpr int counterFont = cv::FONT_HERSHEY_SIMPLEX;
    constexpr const char* counterSample = "Frame: 0000";  // clips are <= 2255 frames

    void render(const cv::Mat& imgScaled, const cv::Mat& mask, int frame_count, const std::vector<Blob>& blobs,
                const cv::Point countLineA, const cv::Point countLineB,
                cv::Mat& mask3c, cv::Mat& frame,
                int crossingCount, const std::vector<Track>& tracks,
                std::optional<int> blobCount = std::nullopt) {
        // Convert mask into 3 channel
        cv::cvtColor(mask, mask3c, cv::COLOR_GRAY2BGR, 3);

        // Concat the img and mask
        cv::hconcat(imgScaled, mask3c, frame);

        // Draw the count lines
        cv::line(frame, countLineA, countLineB, {0, 0, 255}, 2);
        cv::line(frame, countLineA + cv::Point(imgScaled.cols, 0), 
                        countLineB + cv::Point(imgScaled.cols, 0), 
                        {0, 0, 255}, 2);

        // Draw frame counter, sized relative to a 540p frame
        double ratio = static_cast<double>(frame.rows) / refFrameHeight;
        double fontScale = refFontScale * ratio;
        int fontWt = std::max(1, static_cast<int>(std::lround(refFontWt * ratio)));
        int margin = static_cast<int>(std::lround(refMargin * ratio));

        // Draw the labeled boxes
        for (const Blob& blob : blobs) {
            cv::rectangle(frame, blob.bounding_box, cv::Scalar(225, 0, 0), 2);
        }

        // Draw the tracked centroids
        for (const Track& t : tracks) {
            cv::Point pt = cv::Point(t.centroid);
            cv::Scalar color = t.counted ? cv::Scalar(0, 255, 0) : cv::Scalar(128, 128, 0);
            cv::circle(frame, pt, 4, color, -1);
            cv::putText(frame, std::to_string(t.id), 
                pt + cv::Point(2, -2), counterFont, fontScale * 0.4, color, 1);
        }

        // Measure a fixed-width sample so the text doesn't shift left as digits are added
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(counterSample, counterFont, fontScale, fontWt, &baseline);
        // putText's origin is the bottom-left of the baseline; descenders hang below by `baseline`
        cv::Point frameCtCoords(frame.cols - textSize.width - margin, frame.rows - margin - baseline);
        cv::putText(frame, "Frame: " + std::to_string(frame_count),
            frameCtCoords, counterFont, fontScale, {0, 255, 0}, fontWt);

        // Display crossing counts
        cv::putText(frame, "Crossings: " + std::to_string(crossingCount),
            frameCtCoords - cv::Point(0, textSize.height + baseline), counterFont, fontScale, {0, 255, 0}, fontWt);

        // Optionally display blob counts
        if (blobCount.has_value()) {
            cv::putText(frame, "Blobs: " + std::to_string(blobCount.value()),
            frameCtCoords - cv::Point(0, 2*(textSize.height + baseline)), counterFont, fontScale, {0, 255, 0}, fontWt);
        }

        // Draw next frame
        cv::imshow("main", frame);

    }
    enum class KeyAction { Continue, Quit };
    constexpr int KEY_ESC = 27;
    constexpr int KEY_SPACE = ' ';
    bool isQuitKey(int keyCode) {return keyCode == KEY_ESC || keyCode == 'q' || keyCode == 'Q';}
    KeyAction handleInput() {
        int wait = cv::waitKey(1);
        // Listen for stop key
        if (isQuitKey(wait)) {
            return KeyAction::Quit;
        }
        // Listen for pause key
        if (wait == KEY_SPACE) {
            // Enter wait loop
            while (true) {
                int pauseKey = cv::waitKey(0);
                
                // When a key is pressed:
                if (pauseKey == KEY_SPACE) {
                    // Resume loop
                    break;
                } else if (isQuitKey(pauseKey)) {
                    return KeyAction::Quit;
                } else {
                    // non-mapped key, continue in loop
                    std::cout << "Press 'q', 'SPACEBAR', or 'ESC'" << std::endl;
                }
            }
        }
        return KeyAction::Continue;
    }
    constexpr int openKernelSize = 3;
    constexpr int closeKernelSize = 7;
} // namespace

int main(int argc, char* argv[]) {
    // Parse arguments
    InputFlags flags;
    try {
        flags = argParse(argc, argv);
    } catch (const std::runtime_error& e) {
        std::cerr << "An error occured: " << e.what() << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Load file from path
    cv::VideoCapture cap = cv::VideoCapture(flags.video_path);

    // Test for successful path open
    if (cap.isOpened()) {
        std::cout << "Capture Live" << std::endl;
        // Print video properties
        std::cout << "W: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) 
            << " H: " << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;
        std::cout << "Frame count: " << cap.get(cv::CAP_PROP_FRAME_COUNT) << std::endl;
        std::cout << "Frame rate: " << cap.get(cv::CAP_PROP_FPS) << std::endl;
        std::cout << "Duration (sec): " << cap.get(cv::CAP_PROP_FRAME_COUNT) / cap.get(cv::CAP_PROP_FPS) << std::endl;
    } else {
        std::cerr << "Capture failed for provided path " << flags.video_path << std::endl;
        return 1;
    }
    // Load in metadata
    ClipMeta metaData;
    try {
        metaData = loadClipMeta(metadataPath, flags.video_path);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    // Check clip meta against loaded clip
    int clipH = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    int clipW = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    if (metaData.width != clipW || metaData.height != clipH) {
        std::string error_message = "Clip height or width does not match recorded meta for " + flags.video_path;
        std::cerr << error_message << std::endl;
        return 1;
    }

    // Measure time (start stopwatch)
    auto start = std::chrono::steady_clock::now();
    
    // Keep a frame count for metrics later
    int frame_count = 0;
    
    // Instantiate img array
    cv::Mat img;
    // Instantiate resize array
    cv::Mat imgScaled;
    // Initialize the MOG2 background subtractor
    cv::Ptr<cv::BackgroundSubtractorMOG2> mog2 = cv::createBackgroundSubtractorMOG2(mog2History, mog2VarThreshold, true);
    // Initialize mask object
    cv::Mat mask;
    // Initialize binary mask
    cv::Mat binMask;
    // Initialize 3c converted mask
    cv::Mat mask3c;
    // Initialize frame
    cv::Mat frame;

    // Instantiate structures for spatial filtering
    cv::Mat openKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(openKernelSize,openKernelSize));
    cv::Mat closeKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(closeKernelSize,closeKernelSize));

    // Processing Mask
    cv::Mat filterMask; 

    // Initialize Labels
    cv::Mat labels;
    std::vector<Blob> blobs;

    // Scale full-res count-line to flags.scale
    cv::Point countLine1 = metaData.countLine.countLineA * flags.scale;
    cv::Point countLine2 = metaData.countLine.countLineB * flags.scale;

    // Instantiate tracker
    Tracker crossingTracker{CountLine{countLine1, countLine2}};
    
    // Main loop
    while (cap.read(img)) {
        // Reize and apply mog2
        process(img, mog2, flags.scale, imgScaled, mask);

        // Run binary threshold on mask
        cv::threshold(mask, binMask, 200, 255, cv::THRESH_BINARY);

        // Run spatial filtering pass - Morphology
        cv::morphologyEx(binMask, filterMask, cv::MORPH_OPEN, openKernel);
        cv::morphologyEx(filterMask, filterMask, cv::MORPH_CLOSE, closeKernel);

        // Save blobs in mask
        label(filterMask, labels, blobs, flags.kneighbors);
        frame_count++;

        // Filter out small blobs
        blobs.erase(
            std::remove_if(blobs.begin(), blobs.end(), [](const Blob& b) {
                return b.area < blobAreaThreshold;
            }),
            blobs.end()
        );

        // Run Non-max suppression
        std::vector<Blob> keptBlobs = nms(blobs, iouThreshold);

        // Call update to crossing
        crossingTracker.update(keptBlobs, frame_count > warmupFrames);

        if (!flags.headless) {
            // Draw next frame
            render(imgScaled, filterMask, frame_count, keptBlobs, 
                countLine1,
                countLine2, 
                mask3c, frame,
                crossingTracker.count(), crossingTracker.tracks(),
                keptBlobs.size());

            // Handle any inputs
            KeyAction action = handleInput();
            if (action == KeyAction::Quit) {
                break;
            }
        }
    }

    // Report timings
    std::chrono::duration<double> dur = std::chrono::steady_clock::now() - start;
    std::cout << "Run time: "  << dur.count() << std::endl;
    std::cout << "Effective frame rate: "  << frame_count / dur.count() << std::endl;

    // Cleanup
    cv::destroyAllWindows();
    return 0;
}