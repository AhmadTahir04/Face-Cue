#pragma once
#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include "ffa/config.hpp"
#include "ffa/recognizer.hpp"
#include "ffa/storage.hpp"

namespace ffa {

enum class Decision {
    Match,    // confident + unambiguous → may announce
    Unknown,  // not close to anyone enrolled
    Unsure,   // close, but ambiguous (two people too similar) — stay silent
    LowQuality // crop failed the quality gate — don't even match
};

struct MatchResult {
    Decision    decision = Decision::Unknown;
    int64_t     personId = 0;
    std::string name;
    std::string reminder;
    double      score       = 0.0;  // cosine to best person (NOT a probability)
    double      runnerUp    = 0.0;  // cosine to 2nd-best person
};

// Quality gate: is this crop good enough to even attempt a match?
// Returns true if usable. Reason (for logging) written to `reason`.
bool passesQualityGate(const cv::Mat& frameBGR, const DetectedFace& face,
                       const Config& cfg, std::string& reason);

// Compare one query embedding against all enrolled embeddings and apply the
// threshold + margin rules. Does NOT apply multi-frame logic (see FrameVoter).
MatchResult classify(const cv::Mat& queryEmbedding,
                     const std::vector<StoredEmbedding>& enrolled,
                     const std::vector<Person>& people,
                     Recognizer& rec,
                     const Config& cfg);

// Requires the same person to win N consecutive frames before we trust it.
// One unstable frame cannot trigger an announcement.
class FrameVoter {
public:
    explicit FrameVoter(int framesNeeded) : need_(framesNeeded) {}

    // Feed each frame's result; returns true only when the same person has
    // been the Match decision for `framesNeeded` consecutive frames.
    bool accept(const MatchResult& r);
    void reset();

private:
    int     need_;
    int64_t currentId_ = 0;
    int     streak_    = 0;
};

}  // namespace ffa
