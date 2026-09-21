#include "ffa/safety.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <map>

namespace ffa {

bool passesQualityGate(const cv::Mat& frameBGR, const DetectedFace& face,
                       const Config& cfg, std::string& reason) {
    // 1) detector confidence
    if (face.detScore < cfg.detScoreThreshold) {
        reason = "low detector score";
        return false;
    }
    // 2) face too small (far away / tiny) → unreliable
    if (std::min(face.box.width, face.box.height) < cfg.minFacePx) {
        reason = "face too small";
        return false;
    }
    // 3) box must lie within the frame
    cv::Rect safe = face.box & cv::Rect(0, 0, frameBGR.cols, frameBGR.rows);
    if (safe.width < cfg.minFacePx || safe.height < cfg.minFacePx) {
        reason = "face partly out of frame";
        return false;
    }
    // 4) blur check — variance of the Laplacian on the crop
    cv::Mat gray, lap;
    cv::cvtColor(frameBGR(safe), gray, cv::COLOR_BGR2GRAY);
    cv::Laplacian(gray, lap, CV_64F);
    cv::Scalar mean, stddev;
    cv::meanStdDev(lap, mean, stddev);
    double var = stddev[0] * stddev[0];
    if (var < cfg.minBlurVar) {
        reason = "too blurry";
        return false;
    }
    reason = "ok";
    return true;
}

MatchResult classify(const cv::Mat& queryEmbedding,
                     const std::vector<StoredEmbedding>& enrolled,
                     const std::vector<Person>& people,
                     Recognizer& rec,
                     const Config& cfg) {
    MatchResult r;
    if (enrolled.empty()) {
        r.decision = Decision::Unknown;
        return r;
    }

    // Best cosine score per person (a person may have several embeddings).
    std::map<int64_t, double> bestPerPerson;
    for (const auto& e : enrolled) {
        double s = rec.cosine(queryEmbedding, e.vec);
        auto it = bestPerPerson.find(e.personId);
        if (it == bestPerPerson.end() || s > it->second)
            bestPerPerson[e.personId] = s;
    }

    // Rank people by their best score.
    int64_t bestId = 0, runnerId = 0;
    double  best = -2.0, runner = -2.0;
    for (const auto& [pid, s] : bestPerPerson) {
        if (s > best) { runner = best; runnerId = bestId; best = s; bestId = pid; }
        else if (s > runner) { runner = s; runnerId = pid; }
    }

    r.score    = best;
    r.runnerUp = (runnerId ? runner : 0.0);
    r.personId = bestId;
    for (const auto& p : people)
        if (p.id == bestId) { r.name = p.name; r.reminder = p.reminder; break; }

    // Rule 1: must clear the similarity threshold.
    if (best < cfg.cosThreshold) {
        r.decision = Decision::Unknown;
        return r;
    }
    // Rule 2: must beat the 2nd-best person by the margin, else it's ambiguous.
    if (runnerId != 0 && (best - runner) < cfg.margin) {
        r.decision = Decision::Unsure;
        return r;
    }
    r.decision = Decision::Match;
    return r;
}

bool FrameVoter::accept(const MatchResult& r) {
    if (r.decision != Decision::Match) {
        reset();
        return false;
    }
    if (r.personId == currentId_) {
        ++streak_;
    } else {
        currentId_ = r.personId;
        streak_    = 1;
    }
    if (streak_ >= need_) {
        // require the person to leave/return (or lose a frame) before firing again
        streak_ = 0;
        return true;
    }
    return false;
}

void FrameVoter::reset() {
    currentId_ = 0;
    streak_    = 0;
}

}  // namespace ffa
