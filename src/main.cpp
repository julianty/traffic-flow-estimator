#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
#include <opencv2/imgproc.hpp>
#include"args.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <vector>
#include <queue>

namespace {
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
        mog2->apply(imgScaled, mask);
        
    }
    const int blobAreaThreshold = 100;
    struct Blob
    {
        cv::Rect bounding_box;
        int area;
        cv::Point centroid;
    };
    const cv::Point kNeighbors4[] = { {0,-1}, {-1,0}, {1,0}, {0,1} };
    const cv::Point kNeighbors8[] = { {-1,-1}, {0,-1}, {1,-1}, 
                                        {-1,0}, {1,0},
                                        {-1,1}, {0,1}, {1,1} };
    Blob floodFill(const cv::Mat& mask, cv::Mat& labels, const cv::Point& seed, 
                    int labelID, std::queue<cv::Point>& queue) {
        // Label seed
        labels.at<int>(seed.y, seed.x) = labelID;
        queue.push(cv::Point(seed.x, seed.y));

        // Create the blob tracking stats
        int minX = seed.x, maxX = seed.x;
        int minY = seed.y, maxY = seed.y;
        int count(0);
        int sumX = 0, sumY = 0;

        // Process the queue
        while (!queue.empty())  {
            cv::Point pt = queue.front();
            queue.pop();
            
            for (const auto& neighbor : kNeighbors4) {
                cv::Point newPt = cv::Point(neighbor.x + pt.x, neighbor.y + pt.y);
                // Check out of bounds
                if (newPt.x == -1 || newPt.x == mask.cols) continue;
                if (newPt.y == -1 || newPt.y == mask.rows) continue;

                if (mask.at<uchar>(newPt) != 0 && labels.at<int>(newPt) == 0) {
                    // This label can inherit the last label
                    labels.at<int>(newPt) = labelID;
                    // Enqueue its neighbors
                    queue.push(newPt);
                }
            }
            // Update blob stats
            if (pt.x < minX) minX = pt.x;
            if (pt.x > maxX) maxX = pt.x;
            if (pt.y < minY) minY = pt.y;
            if (pt.y > maxY) maxY = pt.y;
            sumX += pt.x;
            sumY += pt.y;
            count++;
            
        }
        // Compute blob
        Blob blob = {cv::Rect(minX, minY, maxX - minX + 1, maxY - minY + 1), 
            count, 
            cv::Point(sumX / count, sumY / count)};
        return blob;
    }
    void label(const cv::Mat& mask, cv::Mat& labels, std::vector<Blob>& blobs) {
        // Create image
        labels.create(mask.rows, mask.cols, CV_32SC1);
        // Ensure zeroing across iterations
        labels.setTo(0);
        // Empty blobs
        blobs.clear();
        
        int nextLabel(1);
        std::queue<cv::Point> processQueue;
        for (int y=0; y < mask.rows; y++) {
            for (int x=0; x < mask.cols; x++) {
                if (mask.at<uchar>(y, x) != 0 && labels.at<int>(y, x) == 0) {
                    Blob newBlob = floodFill(mask, labels, cv::Point(x,y), nextLabel, processQueue);
                    blobs.push_back(newBlob);
                    nextLabel++;
                }
            }
        }
    }
    constexpr double refFontScale = 1.2;
    constexpr int refFontWt = 2;
    constexpr int refFrameHeight = 540;
    constexpr int refMargin = 20;
    constexpr int counterFont = cv::FONT_HERSHEY_SIMPLEX;
    constexpr const char* counterSample = "Frame: 0000";  // clips are <= 2255 frames
    void render(const cv::Mat& imgScaled, const cv::Mat& mask, int frame_count, const std::vector<Blob>& blobs,
                cv::Mat& mask3c, cv::Mat& frame) {
        // Convert mask into 3 channel
        cv::cvtColor(mask, mask3c, cv::COLOR_GRAY2BGR, 3);

        // Concat the img and mask
        cv::hconcat(imgScaled, mask3c, frame);

        // Draw frame counter, sized relative to a 540p frame
        double ratio = static_cast<double>(frame.rows) / refFrameHeight;
        double fontScale = refFontScale * ratio;
        int fontWt = std::max(1, static_cast<int>(std::lround(refFontWt * ratio)));
        int margin = static_cast<int>(std::lround(refMargin * ratio));

        // Draw the labeled boxes
        for (const Blob& blob : blobs) {
            if (blob.area < blobAreaThreshold) continue;
            cv::rectangle(frame, blob.bounding_box, cv::Scalar(225, 0, 0), 2);
        }

        // Measure a fixed-width sample so the text doesn't shift left as digits are added
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(counterSample, counterFont, fontScale, fontWt, &baseline);
        // putText's origin is the bottom-left of the baseline; descenders hang below by `baseline`
        cv::Point frameCtCoords(frame.cols - textSize.width - margin, frame.rows - margin - baseline);
        cv::putText(frame, "Frame: " + std::to_string(frame_count),
            frameCtCoords, counterFont, fontScale, {0, 255, 0}, fontWt);


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

    // Measure time (start stopwatch)
    auto start = std::chrono::steady_clock::now();
    
    // Keep a frame count for metrics later
    int frame_count = 0;
    
    // Instantiate img array
    cv::Mat img;
    // Instantiate resize array
    cv::Mat imgScaled;
    // Initialize the MOG2 background subtractor
    cv::Ptr<cv::BackgroundSubtractorMOG2> mog2 = cv::createBackgroundSubtractorMOG2(500, 16.0, true);
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
        label(filterMask, labels, blobs);
        frame_count++;

        if (!flags.headless) {
            // Draw next frame
            render(imgScaled, filterMask, frame_count, blobs, mask3c, frame);

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