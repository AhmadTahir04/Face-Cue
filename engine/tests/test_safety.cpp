// Unit tests for the safety-critical decision logic.
// These run WITHOUT the ML model — the decision layer is pure math/rules,
// which is exactly why we can test it deterministically.
#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "ffa/safety.hpp"
#include "ffa/config.hpp"

using namespace ffa;

// --- helpers -------------------------------------------------------------
static cv::Mat vec(std::initializer_list<float> v) {
    cv::Mat m(1, (int)v.size(), CV_32F);
    int i = 0; for (float x : v) m.at<float>(0, i++) = x;
    return m;
}

// ---------------------------- cosineSim ----------------------------------
TEST(CosineSim, IdenticalIsOne) {
    cv::Mat a = vec({1, 2, 3, 4});
    EXPECT_NEAR(cosineSim(a, a), 1.0, 1e-6);
}
TEST(CosineSim, OrthogonalIsZero) {
    EXPECT_NEAR(cosineSim(vec({1, 0}), vec({0, 1})), 0.0, 1e-6);
}
TEST(CosineSim, OppositeIsMinusOne) {
    EXPECT_NEAR(cosineSim(vec({1, 1}), vec({-1, -1})), -1.0, 1e-6);
}

// ---------------------------- classify -----------------------------------
class Classify : public ::testing::Test {
protected:
    Config cfg;
    std::vector<Person> people{{1, "Alice", "friend"}, {2, "Bob", "cousin"}};
    std::vector<StoredEmbedding> enrolled;
    void SetUp() override {
        cfg.cosThreshold = 0.5f;
        cfg.margin       = 0.1f;
        enrolled = {
            {1, vec({1, 0, 0})},   // Alice
            {2, vec({0, 1, 0})},   // Bob
        };
    }
};

TEST_F(Classify, EmptyEnrolledIsUnknown) {
    auto r = classify(vec({1, 0, 0}), {}, people, cfg);
    EXPECT_EQ(r.decision, Decision::Unknown);
}

TEST_F(Classify, ClearMatch) {
    auto r = classify(vec({1, 0, 0}), enrolled, people, cfg);  // == Alice
    EXPECT_EQ(r.decision, Decision::Match);
    EXPECT_EQ(r.personId, 1);
    EXPECT_EQ(r.name, "Alice");
}

TEST_F(Classify, BelowThresholdIsUnknown) {
    // Similar to no one (cos ~0.57 to both, but let's make it clearly low).
    auto r = classify(vec({1, 1, 5}), enrolled, people, cfg);
    EXPECT_EQ(r.decision, Decision::Unknown);
}

TEST_F(Classify, AmbiguousIsUnsure) {
    // Halfway between Alice and Bob: high score to both, tiny margin.
    auto r = classify(vec({1, 1, 0}), enrolled, people, cfg);
    EXPECT_EQ(r.decision, Decision::Unsure);
}

TEST_F(Classify, MarginZeroAllowsCloseCall) {
    cfg.margin = 0.0f;
    auto r = classify(vec({1, 0.9f, 0}), enrolled, people, cfg);
    EXPECT_EQ(r.decision, Decision::Match);  // Alice edges Bob
    EXPECT_EQ(r.personId, 1);
}

// --------------------------- FrameVoter ----------------------------------
static MatchResult matchOf(int64_t id) {
    MatchResult r; r.decision = Decision::Match; r.personId = id; return r;
}

TEST(FrameVoter, RequiresConsecutiveAgreement) {
    FrameVoter v(3);
    EXPECT_FALSE(v.accept(matchOf(1)));
    EXPECT_FALSE(v.accept(matchOf(1)));
    EXPECT_TRUE (v.accept(matchOf(1)));   // 3rd agreeing frame fires
}

TEST(FrameVoter, NonMatchResets) {
    FrameVoter v(3);
    v.accept(matchOf(1));
    v.accept(matchOf(1));
    MatchResult unknown; unknown.decision = Decision::Unknown;
    EXPECT_FALSE(v.accept(unknown));       // resets streak
    EXPECT_FALSE(v.accept(matchOf(1)));
    EXPECT_FALSE(v.accept(matchOf(1)));
    EXPECT_TRUE (v.accept(matchOf(1)));
}

TEST(FrameVoter, DifferentPersonResetsStreak) {
    FrameVoter v(3);
    v.accept(matchOf(1));
    v.accept(matchOf(1));
    EXPECT_FALSE(v.accept(matchOf(2)));    // switched person -> streak=1
    EXPECT_FALSE(v.accept(matchOf(2)));
    EXPECT_TRUE (v.accept(matchOf(2)));
}

// ------------------------- passesQualityGate -----------------------------
static DetectedFace faceAt(cv::Rect box, float score) {
    DetectedFace f; f.box = box; f.detScore = score; return f;
}

TEST(QualityGate, RejectsLowDetectorScore) {
    cv::Mat img(480, 640, CV_8UC3);
    cv::randu(img, 0, 255);
    std::string why;
    EXPECT_FALSE(passesQualityGate(img, faceAt({100, 100, 200, 200}, 0.5f), Config{}, why));
}

TEST(QualityGate, RejectsTinyFace) {
    cv::Mat img(480, 640, CV_8UC3);
    cv::randu(img, 0, 255);
    std::string why;
    EXPECT_FALSE(passesQualityGate(img, faceAt({100, 100, 20, 20}, 0.99f), Config{}, why));
}

TEST(QualityGate, RejectsBlurryFlatImage) {
    cv::Mat img(480, 640, CV_8UC3, cv::Scalar(120, 120, 120));  // flat -> no detail
    std::string why;
    EXPECT_FALSE(passesQualityGate(img, faceAt({100, 100, 200, 200}, 0.99f), Config{}, why));
    EXPECT_EQ(why, "too blurry");
}

TEST(QualityGate, AcceptsGoodFace) {
    cv::Mat img(480, 640, CV_8UC3);
    cv::randu(img, 0, 255);  // lots of high-frequency detail -> high Laplacian var
    std::string why;
    EXPECT_TRUE(passesQualityGate(img, faceAt({100, 100, 200, 200}, 0.99f), Config{}, why));
}
