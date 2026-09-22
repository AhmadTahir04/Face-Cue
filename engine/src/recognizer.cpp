#include "ffa/recognizer.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry.hpp>   // OpenCV 5: getRotationMatrix2D, invertAffineTransform
#include <stdexcept>
#include <algorithm>

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

    tiltAngles_ = cfg.tiltAngles;
}

std::vector<DetectedFace> Recognizer::detectRaw(const cv::Mat& frameBGR) {
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

std::vector<DetectedFace> Recognizer::detect(const cv::Mat& frameBGR) {
    // First try the frame as-is (fast path, handles most cases).
    auto faces = detectRaw(frameBGR);
    if (!faces.empty() || frameBGR.empty()) return faces;

    // Tilt fallback: the face may be rotated in-plane (crooked camera/head).
    // Rotate the frame by each angle, detect, then map the box + 5 landmarks
    // back into the original image so everything downstream is unchanged.
    cv::Point2f center(frameBGR.cols / 2.f, frameBGR.rows / 2.f);
    for (int angle : tiltAngles_) {
        cv::Mat M = cv::getRotationMatrix2D(center, angle, 1.0);
        cv::Mat rotated;
        cv::warpAffine(frameBGR, rotated, M, frameBGR.size());
        auto rf = detectRaw(rotated);
        if (rf.empty()) continue;

        cv::Mat Minv;
        cv::invertAffineTransform(M, Minv);  // rotated -> original
        const double* m = Minv.ptr<double>();
        auto mapPt = [&](float x, float y) {
            return cv::Point2f(float(m[0] * x + m[1] * y + m[2]),
                               float(m[3] * x + m[4] * y + m[5]));
        };

        std::vector<DetectedFace> out;
        for (auto& f : rf) {
            cv::Mat row = f.detRow.clone();
            float bx = row.at<float>(0), by = row.at<float>(1);
            float bw = row.at<float>(2), bh = row.at<float>(3);
            cv::Point2f c[4] = { mapPt(bx, by), mapPt(bx + bw, by),
                                 mapPt(bx, by + bh), mapPt(bx + bw, by + bh) };
            float minx = c[0].x, maxx = c[0].x, miny = c[0].y, maxy = c[0].y;
            for (int k = 1; k < 4; ++k) {
                minx = std::min(minx, c[k].x); maxx = std::max(maxx, c[k].x);
                miny = std::min(miny, c[k].y); maxy = std::max(maxy, c[k].y);
            }
            row.at<float>(0) = minx; row.at<float>(1) = miny;
            row.at<float>(2) = maxx - minx; row.at<float>(3) = maxy - miny;
            // Map the 5 landmarks (used by alignCrop) back to original coords.
            for (int k = 0; k < 5; ++k) {
                cv::Point2f p = mapPt(row.at<float>(4 + 2 * k), row.at<float>(5 + 2 * k));
                row.at<float>(4 + 2 * k) = p.x;
                row.at<float>(5 + 2 * k) = p.y;
            }
            DetectedFace nf;
            nf.detRow   = row;
            nf.detScore = f.detScore;
            nf.box = cv::Rect(cvRound(minx), cvRound(miny),
                              cvRound(maxx - minx), cvRound(maxy - miny));
            out.push_back(std::move(nf));
        }
        return out;  // first angle that finds a face wins
    }
    return {};
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
