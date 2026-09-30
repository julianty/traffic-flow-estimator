#include <opencv2/opencv.hpp>
#include <iostream>
int main() {
    std::cout << "OpenCV " << CV_VERSION << "\n";
    cv::Mat img(240, 320, CV_8UC3, cv::Scalar(40, 40, 40));
    cv::putText(img, "it works", {60, 130}, cv::FONT_HERSHEY_SIMPLEX, 1.2, {0, 255, 0}, 2);
    cv::imshow("test", img);
    cv::waitKey(0);
}