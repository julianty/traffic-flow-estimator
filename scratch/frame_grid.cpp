// Dump one full-res frame with a labeled pixel grid, for reading count-line coordinates.
// Usage: frame_grid <video> [frame=150] [out=build/frame_grid.png] [step=100]
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: frame_grid <video> [frame=150] [out=build/frame_grid.png] [step=100]\n";
        return 1;
    }
    int target = argc > 2 ? std::stoi(argv[2]) : 150;
    std::string out = argc > 3 ? argv[3] : "build/frame_grid.png";
    int step = argc > 4 ? std::stoi(argv[4]) : 100;

    cv::VideoCapture cap(argv[1]);
    if (!cap.isOpened()) {
        std::cerr << "cannot open " << argv[1] << "\n";
        return 1;
    }
    cv::Mat img;
    for (int i = 0; i < target; ++i) {
        if (!cap.read(img)) {
            std::cerr << "clip ended before frame " << target << "\n";
            return 1;
        }
    }

    const cv::Scalar lineColor(0, 255, 255);
    const cv::Scalar textColor(0, 255, 0);
    for (int x = 0; x < img.cols; x += step) {
        cv::line(img, {x, 0}, {x, img.rows - 1}, lineColor, 1);
        cv::putText(img, std::to_string(x), {x + 3, 18}, cv::FONT_HERSHEY_SIMPLEX, 0.55, textColor, 1);
    }
    for (int y = 0; y < img.rows; y += step) {
        cv::line(img, {0, y}, {img.cols - 1, y}, lineColor, 1);
        cv::putText(img, std::to_string(y), {3, y - 4}, cv::FONT_HERSHEY_SIMPLEX, 0.55, textColor, 1);
    }

    cv::imwrite(out, img);
    std::cout << "wrote " << out << " (" << img.cols << "x" << img.rows << ", frame " << target << ")\n";
    return 0;
}
