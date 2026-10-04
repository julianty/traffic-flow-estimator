#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
#include <opencv2/imgproc.hpp>
#include"args.hpp"



int main(int argc, char* argv[]) {
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
    std::chrono::steady_clock clock;
    std::chrono::time_point<std::chrono::steady_clock>  start = clock.now();
    
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
    bool quit = false;
    while (!quit && cap.read(img)) {
        // Resize image
        cv::resize(img, imgScaled, cv::Size(), flags.scale, flags.scale, cv::INTER_AREA);

        // Run MOG2
        mog2->apply(imgScaled, mask);

        // Increment frame count
        frame_count++;


        if (!flags.headless) {
            // Convert mask into 3 channel
            cv::cvtColor(mask, mask3c, cv::COLOR_GRAY2RGB, 3);
    
            // Concat the img and mask
            cv::hconcat(imgScaled, mask3c, frame);
    
            // Draw next frame
            cv::putText(frame, "Frame: " + std::to_string(frame_count), 
                        {1730,500}, cv::FONT_HERSHEY_SIMPLEX, 1.2, 
                        {0, 255, 0}, 2);
            cv::imshow("main", frame);
            int wait = cv::waitKey(1);
            // Listen for stop key
            if (wait == 27 || wait == 113 || wait == 81) {
                // ESC = 27, q = 113, Q = 81
                // Exit the loop
                quit = true;
            }
            // Listen for pause key
            if (wait == 32) {
                // SPACEBAR = 32
                // Enter wait loop
                while (1) {
                    int pauseKey = cv::waitKey(0);
                    
                    // When a key is pressed:
                    if (pauseKey == 32) {
                        // Resume loop
                        break;
                    } else if (pauseKey == 27 || pauseKey == 113 || pauseKey== 81) {
                        // ESC = 27, q = 113, Q = 81
                        // Exit the loop
                        quit = true;
                        break;
                    } else {
                        // non-mapped key, continue in loop
                        std::cout << "Press 'q', 'SPACEBAR', or 'ESC'" << std::endl;
                        continue;
                    }
                }
            }
        }


    }

    // Report timings
    std::chrono::time_point<std::chrono::steady_clock>  stop = clock.now();
    std::chrono::duration<double> dur = stop - start;
    std::cout << "Run time: "  << dur.count() << std::endl;
    std::cout << "Effective frame rate: "  << frame_count / dur.count() << std::endl;

    // Cleanup
    cv::destroyAllWindows();
    return 0;
}