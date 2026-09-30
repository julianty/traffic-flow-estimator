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
    return 0;
}