#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: main <video_path>" << endl;
        return 1;
    }
    string file_path = argv[1];

    // Load file from path
    cv::VideoCapture cap = cv::VideoCapture(file_path);
    // Test for successful path open
    if (cap.isOpened()) {
        cout << "Capture Live" << endl;
        // Print video properties
        cout << "W: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) 
            << " H: " << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << endl;
        cout << "Frame count: " << cap.get(cv::CAP_PROP_FRAME_COUNT) << endl;
        cout << "Frame rate: " << cap.get(cv::CAP_PROP_FPS) << endl;
        cout << "Duration (sec): " << cap.get(cv::CAP_PROP_FRAME_COUNT) / cap.get(cv::CAP_PROP_FPS) << endl;
    } else {
        cerr << "Capture failed for provided path " << file_path << endl;
        return 1;
    }

    // Get next frame
    cv::Mat img;
    // Measure time
    std::chrono::steady_clock clock;
    std::chrono::time_point<std::chrono::steady_clock>  start = clock.now();
    int frame_count = 0;
    while (cap.read(img)) {
        // Increment frame count
        frame_count++;
        // Draw next frame
        cv::imshow("main", img);
        // Calculate frame time
        double frame_time_ms = 1000 / cap.get(cv::CAP_PROP_FPS);
        int wait = cv::waitKey(static_cast<int>(frame_time_ms));
        // Listen for stop key
        if (wait == 27 || wait == 113 || wait == 81) {
            // ESC = 27, q = 113, Q = 81
            // Exit the loop
            break;
        }
    }

    // Report timings
    std::chrono::time_point<std::chrono::steady_clock>  stop = clock.now();
    std::chrono::duration<double> dur = stop - start;
    cout << "Run time: "  << dur.count() << endl;
    cout << "Effective frame rate: "  << cap.get(cv::CAP_PROP_FRAME_COUNT) / dur.count() << endl;

    // Cleanup
    cv::destroyAllWindows();
    return 0;
}