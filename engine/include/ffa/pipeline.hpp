#pragma once
#include <opencv2/core.hpp>
#include <string>
#include "ffa/config.hpp"
#include "ffa/recognizer.hpp"

namespace ffa {

// Detect the largest face in a BGR frame that passes the quality gate and
// return its 128-D embedding. Returns an empty Mat if no usable face is found;
// `reason` explains why. Shared by the live camera path, the offline image
// commands, and the tuning/evaluation tool so they all behave identically.
cv::Mat bestFaceEmbedding(const cv::Mat& frameBGR, Recognizer& rec,
                          const Config& cfg, std::string& reason);

}  // namespace ffa
