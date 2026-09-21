#include "ffa/camera.hpp"

namespace ffa {

Camera::Camera(int index) {
    cap_.open(index);
    if (cap_.isOpened()) {
        cap_.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
        cap_.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    }
}

Camera::~Camera() {
    if (cap_.isOpened()) cap_.release();
}

bool Camera::isOpen() const { return cap_.isOpened(); }

bool Camera::read(cv::Mat& frame) {
    if (!cap_.isOpened()) return false;
    return cap_.read(frame) && !frame.empty();
}

int Camera::width() const  { return static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH)); }
int Camera::height() const { return static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT)); }

}  // namespace ffa
