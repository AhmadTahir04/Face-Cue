#pragma once
#include <opencv2/core.hpp>
#include <opencv2/objdetect/face.hpp>
#include <vector>
#include "ffa/config.hpp"

namespace ffa {

// One detected face in a frame.
struct DetectedFace {
    cv::Rect box;            // bounding box in image pixels
    float    detScore = 0;   // detector confidence [0,1]
    cv::Mat  detRow;         // 1x15 detector output row (needed for alignCrop)
};

// Wraps YuNet (detection) + SFace (128-D embeddings). This is the only place
// that touches the ML models.
class Recognizer {
public:
    explicit Recognizer(const Config& cfg);

    // Detect all faces in a BGR frame.
    std::vector<DetectedFace> detect(const cv::Mat& frameBGR);

    // Align/crop the given face and return its 128-D embedding (1x128 CV_32F).
    cv::Mat embed(const cv::Mat& frameBGR, const DetectedFace& face);

    // Cosine similarity between two embeddings (higher = more similar).
    double cosine(const cv::Mat& a, const cv::Mat& b);

private:
    // Single-pass detection on the frame as-is (no rotation).
    std::vector<DetectedFace> detectRaw(const cv::Mat& frameBGR);

    cv::Ptr<cv::FaceDetectorYN>   detector_;
    cv::Ptr<cv::FaceRecognizerSF> recognizer_;
    cv::Size lastInputSize_{0, 0};
    std::vector<int> tiltAngles_;
};

}  // namespace ffa
