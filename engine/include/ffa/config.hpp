#pragma once
#include <string>

namespace ffa {

// All tunables in one place. These are UNCALIBRATED starting defaults — they
// will be tuned on one data split and evaluated on a separate split during the
// Evaluation milestone. A cosine score is NOT a calibrated probability.
struct Config {
    // --- model files (downloaded by scripts/download_models.sh) ---
    // Paths are relative to engine/build (where the binaries run).
    std::string detectorModel   = "../models/face_detection_yunet_2023mar.onnx";
    std::string recognizerModel = "../models/face_recognition_sface_2021dec.onnx";

    // --- camera ---
    int cameraIndex = 0;  // which webcam (0 = default; try 1, 2 if the wrong one opens)

    // --- storage ---
    std::string dbPath = "../../data/faces.db";  // repo-root/data, from engine/build

    // --- detector (YuNet) ---
    float detScoreThreshold = 0.90f;  // minimum detector confidence to consider a face
    float nmsThreshold      = 0.30f;
    int   topK              = 50;

    // --- quality gate (reject bad crops before matching) ---
    int    minFacePx   = 80;     // reject faces smaller than this (min of w,h), px
    double minBlurVar  = 40.0;   // reject crops with Laplacian variance below this (blurry)

    // --- matching (SFace cosine similarity) ---
    // Tuned on LFW (see docs/EVALUATION.md): 0.54/0.10 gave 0 false matches over
    // 1642 stranger tests and 0 wrong names, at the cost of ~0.5% misses.
    // OpenCV's default 0.363 gave 42 false matches on the same set — far too
    // risky for this product. Re-tune on your own enrolled group for real use.
    float cosThreshold = 0.54f;  // face must be at least this similar to an enrolled person
    float margin       = 0.10f;  // best person must beat 2nd-best person by this much

    // --- multi-frame agreement ---
    int framesNeeded = 5;        // consecutive agreeing frames before announcing

    // --- announcer ---
    int    cooldownSeconds = 120;  // don't re-announce the same person within this window
    bool   speak           = true; // use macOS `say` for audio (falls back to print)

    // --- enrollment ---
    int enrollShots = 5;         // how many good embeddings to capture per person
};

}  // namespace ffa
