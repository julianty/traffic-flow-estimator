#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
#include <opencv2/imgproc.hpp>
#include"args.hpp"
#include <chrono>

namespace {
    // Parameter order (both functions): inputs, then outputs
    void process(const cv::Mat& img, const cv::Ptr<cv::BackgroundSubtractorMOG2>& mog2, double scale,
                 cv::Mat& imgScaled, cv::Mat& mask) {
        // Resize image
        cv::resize(img, imgScaled, cv::Size(), scale, scale, cv::INTER_AREA);

        // Run MOG2
        mog2->apply(imgScaled, mask);
    }
    void render(const cv::Mat& imgScaled, const cv::Mat& mask, int frame_count,
                cv::Mat& mask3c, cv::Mat& frame) {
        // Convert mask into 3 channel
        cv::cvtColor(mask, mask3c, cv::COLOR_GRAY2BGR, 3);

        // Concat the img and mask
        cv::hconcat(imgScaled, mask3c, frame);

        // Draw frame counter
        cv::Point2d frameCtCoords = cv::Point2d(frame.cols - 220, frame.rows - 30);
        cv::putText(frame, "Frame: " + std::to_string(frame_count), 
            frameCtCoords, cv::FONT_HERSHEY_SIMPLEX, 1.2, 
            {0, 255, 0}, 2);
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
            while (1) {
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
    // Initialize 3c converted mask
    cv::Mat mask3c;
    // Initialize frame
    cv::Mat frame;


    // Main loop
    while (cap.read(img)) {
        // Reize and apply mog2
        process(img, mog2, flags.scale, imgScaled, mask);
        frame_count++;

        if (!flags.headless) {
            // Draw next frame
            render(imgScaled, mask, frame_count, mask3c, frame);

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