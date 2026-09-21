#include "ffa/recognizer.hpp"
#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace ffa {

Recognizer::Recognizer(const Config& cfg) {
    // input size is updated per-frame via setInputSize().
    detector_ = cv::FaceDetectorYN::create(
        cfg.detectorModel, "", cv::Size(320, 320),
        cfg.detScoreThreshold, cfg.nmsThreshold, cfg.topK);
    if (detector_.empty())
        throw std::runtime_error("Failed to load detector model: " + cfg.detectorModel);

    recognizer_ = cv::FaceRecognizerSF::create(cfg.recognizerModel, "");
    if (recognizer_.empty())
        throw std::runtime_error("Failed to load recognizer model: " + cfg.recognizerModel);
}

std::vector<DetectedFace> Recognizer::detect(const cv::Mat& frameBGR) {
    std::vector<DetectedFace> out;
    if (frameBGR.empty()) return out;

    cv::Size sz(frameBGR.cols, frameBGR.rows);
    if (sz != lastInputSize_) {
        detector_->setInputSize(sz);
        lastInputSize_ = sz;
    }

    cv::Mat faces;
    detector_->detect(frameBGR, faces);  // Nx15: [x,y,w,h, 5x(lm x,y), score]
    for (int i = 0; i < faces.rows; ++i) {
        DetectedFace f;
        f.detRow   = faces.row(i).clone();
        f.box      = cv::Rect(
            cv::Point(cvRound(faces.at<float>(i, 0)), cvRound(faces.at<float>(i, 1))),
            cv::Size(cvRound(faces.at<float>(i, 2)), cvRound(faces.at<float>(i, 3))));
        f.detScore = faces.at<float>(i, 14);
        out.push_back(std::move(f));
    }
    return out;
}

cv::Mat Recognizer::embed(const cv::Mat& frameBGR, const DetectedFace& face) {
    cv::Mat aligned, feature;
    recognizer_->alignCrop(frameBGR, face.detRow, aligned);
    recognizer_->feature(aligned, feature);
    return feature.clone();  // 1x128 CV_32F
}

double Recognizer::cosine(const cv::Mat& a, const cv::Mat& b) {
    return recognizer_->match(a, b, cv::FaceRecognizerSF::FR_COSINE);
}

}  // namespace ffa
