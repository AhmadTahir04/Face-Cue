#pragma once
#include <opencv2/videoio.hpp>
#include <opencv2/core.hpp>

namespace ffa {

// Thin wrapper around a frame source. Today it is the laptop webcam.
// For the future wearable, ONLY this class changes (e.g. read an MJPEG stream
// from a phone/Pi) — the rest of the pipeline stays identical.
class Camera {
public:
    explicit Camera(int index = 0);
    ~Camera();

    bool isOpen() const;
    // Grabs the next frame. Returns false if the camera failed.
    bool read(cv::Mat& frame);

    int width() const;
    int height() const;

private:
    cv::VideoCapture cap_;
};

}  // namespace ffa
