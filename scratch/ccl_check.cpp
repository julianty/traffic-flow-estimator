#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
#include <opencv2/imgproc.hpp>
#include "../src/ccl.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <vector>
#include <queue>

namespace {
    constexpr int openKernelSize = 3;
    constexpr int closeKernelSize = 7;
    const std::string main_video_path = "source-videos/16516296_1920_1080_25fps.mp4";
    const std::string ver_video_path = "source-videos/16516297_1920_1080_25fps.mp4";
    constexpr double scale = 1.0;
    constexpr int KnTest = 8;
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

    bool compareBlobs(const std::vector<Blob>& ff_blobs, const std::vector<Blob>& cv_blobs, int frame) {
        bool ok = true;
        if (ff_blobs.size() != cv_blobs.size()) {
            std::cout << "Frame " << frame << " Size mismatch" << std::endl;
            std::cout << "ff: " << ff_blobs.size() << " vs cv: " << cv_blobs.size() << std::endl;
            return false;
        }
        for (size_t i=0; i < ff_blobs.size(); i++) {
            if (ff_blobs[i].bounding_box != cv_blobs[i].bounding_box) {
                std::cout << ff_blobs[i].bounding_box << " != " << cv_blobs[i].bounding_box << std::endl;
                std::cout << "Frame " << frame << std::endl;
                ok = false;
            }
            if (ff_blobs[i].area != cv_blobs[i].area) {
                std::cout << ff_blobs[i].area << " != " << cv_blobs[i].area << std::endl;
                std::cout << "Frame " << frame << std::endl;
                ok = false;
            }
            if (ff_blobs[i].centroid != cv_blobs[i].centroid) {
                std::cout << ff_blobs[i].centroid << " != " << cv_blobs[i].centroid << std::endl;
                std::cout << "Frame " << frame << std::endl;
                ok = false;
            }
        }
        return ok;
    }
} // namespace

int main(int argc, char* argv[]) {
    // Load file from path
    cv::VideoCapture cap = cv::VideoCapture(ver_video_path);

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
        std::cerr << "Capture failed for provided path " << ver_video_path << std::endl;
        return 1;
    }

    // Measure time (start stopwatch)
    auto start = std::chrono::steady_clock::now();
    
    // Keep a frame count for metrics later
    int frame_count = 0;
    int framesChecked = 0;
    int framesMismatched = 0;
    
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

    // Initialize CV labels
    cv::Mat cvLabels;
    cv::Mat stats; 
    cv::Mat centroids;

    // Main loop
    while (cap.read(img)) {
        // Reize and apply mog2
        process(img, mog2, scale, imgScaled, mask);

        // Run binary threshold on mask
        cv::threshold(mask, binMask, 200, 255, cv::THRESH_BINARY);

        // Run spatial filtering pass - Morphology
        cv::morphologyEx(binMask, filterMask, cv::MORPH_OPEN, openKernel);
        cv::morphologyEx(filterMask, filterMask, cv::MORPH_CLOSE, closeKernel);

        // Save blobs in mask
        Neighbors kn = KnTest == 4 ? Neighbors::Four : Neighbors::Eight;
        label(filterMask, labels, blobs, kn);

        // Run CV connected components
        int numLabels = cv::connectedComponentsWithStats(filterMask, cvLabels, stats, centroids, KnTest, CV_32S);
        // Convert output to blobs for comparison
        std::vector<Blob> cvBlobs;
        for (int i=1; i < numLabels; i++) {
            int minX = stats.at<int>(i, cv::CC_STAT_LEFT);
            int minY = stats.at<int>(i, cv::CC_STAT_TOP);
            int width = stats.at<int>(i, cv::CC_STAT_WIDTH);
            int height = stats.at<int>(i, cv::CC_STAT_HEIGHT);

            int count = stats.at<int>(i, cv::CC_STAT_AREA);
            cv::Point centroid = cv::Point(centroids.at<double>(i,0), centroids.at<double>(i,1));

            Blob blob = {cv::Rect(minX, minY, width, height), 
                count,
                centroid};
            cvBlobs.push_back(blob);
        }

        // Sort blobs
        auto sortFunc = [](const Blob& a, const Blob& b) -> bool {
            int a_y = a.bounding_box.y;
            int b_y = b.bounding_box.y;
            if (a_y != b_y) {
                return a_y < b_y;
            } 
            int a_x = a.bounding_box.x;
            int b_x = b.bounding_box.x;
            if (a_x != b_x) {
                return a_x < b_x;
            }
            // Tie-breakers: blobs can share a top-left corner (a small blob inside a
            // larger one's box under 8-connectivity), so std::sort needs a total order
            if (a.area != b.area) {
                return a.area < b.area;
            }
            if (a.bounding_box.width != b.bounding_box.width) {
                return a.bounding_box.width < b.bounding_box.width;
            }
            return a.bounding_box.height < b.bounding_box.height;
        };
        std::sort(blobs.begin(), blobs.end(), sortFunc);
        std::sort(cvBlobs.begin(), cvBlobs.end(), sortFunc);

        // Compare
        bool blobsMatch = compareBlobs(blobs, cvBlobs, frame_count);
        if (!blobsMatch) framesMismatched++;
        framesChecked++;
        frame_count++;

        // Draw next frame
        // render(imgScaled, filterMask, frame_count, blobs, mask3c, frame);

        // Handle any inputs
        // KeyAction action = handleInput();
        // if (action == KeyAction::Quit) {
        //     break;
        // }
    }

    // Report timings
    std::chrono::duration<double> dur = std::chrono::steady_clock::now() - start;
    std::cout << "Run time: "  << dur.count() << std::endl;
    std::cout << "Effective frame rate: "  << frame_count / dur.count() << std::endl;
    std::cout << "Per frame blob check" << std::endl;
    std::cout << "Frames Checked: " << framesChecked 
        << "; Frames w/ mismatched blobs: " << framesMismatched 
        << "; Total blobs = " << blobs.size() << std::endl;
    std::cout << "Kneighbors = " <<  KnTest 
        << ", OpenKernel = " << openKernelSize  
        << ", CloseKernel = " << closeKernelSize << std::endl;

    // Cleanup
    cv::destroyAllWindows();
    return 0;
}