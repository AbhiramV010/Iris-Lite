#include <opencv2/opencv.hpp>
#include "grass_detector.hpp"
#include "compressor.hpp"
#include "utils.hpp"

int main() {
    cv::VideoCapture cap(0);
    Compressor comp;

    bool recording = false;

    while (true) {
        cv::Mat frame;
        cap >> frame;

        bool occluded = isGrassOccluded(frame);
        cv::Mat mask = getForegroundMask(frame);

        if (occluded && !recording) {
            comp.startNewClip(generateFilename());
            recording = true;
        }

        if (!occluded && recording) {
            comp.endClip();
            recording = false;
        }

        if (recording) {
            comp.addFrame(frame, mask);
        }

        cv::imshow("Camera", frame);
        if (cv::waitKey(1) == 27) break; // ESC to exit
    }

    comp.endClip();
    return 0;
}
