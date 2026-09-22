#include "ffa/pipeline.hpp"
#include "ffa/safety.hpp"

namespace ffa {

cv::Mat bestFaceEmbedding(const cv::Mat& frameBGR, Recognizer& rec,
                          const Config& cfg, std::string& reason) {
    if (frameBGR.empty()) { reason = "empty image"; return {}; }

    auto faces = rec.detect(frameBGR);
    if (faces.empty()) { reason = "no face"; return {}; }

    // Largest face = the person the camera is facing.
    int best = -1, area = 0;
    for (size_t i = 0; i < faces.size(); ++i)
        if (faces[i].box.area() > area) { area = faces[i].box.area(); best = int(i); }

    if (!passesQualityGate(frameBGR, faces[best], cfg, reason)) return {};

    reason = "ok";
    return rec.embed(frameBGR, faces[best]);
}

}  // namespace ffa
