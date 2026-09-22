// ffa_eval — offline evaluation & threshold tuning tool.
//
// Runs the REAL recognition pipeline over a labeled image dataset, sweeps the
// cosine threshold and margin, and reports the metrics that matter for this
// product — especially the two failures we must minimize:
//   * wrong-name matches (enrolled person announced as someone else)
//   * false matches      (a stranger announced as an enrolled person)
//
// Dataset layout (one subfolder per identity):
//   root/
//     Alice/ img1.jpg img2.jpg ...
//     Bob/   ...
//
// Some identities are "enrolled"; the rest are held out as strangers (negative
// cases). For enrolled identities, a few images become the enrolled templates
// and the remaining images are positive test queries.
//
// Usage:
//   ffa_eval <root> [--enroll-per N] [--min-images M]
//            [--enrolled K] [--max-strangers S] [--report PATH]
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <chrono>

#include "ffa/config.hpp"
#include "ffa/recognizer.hpp"
#include "ffa/safety.hpp"
#include "ffa/pipeline.hpp"

namespace fs = std::filesystem;
using namespace ffa;

namespace {

std::string flag(const std::vector<std::string>& a, const std::string& f, const std::string& d) {
    for (size_t i = 0; i + 1 < a.size(); ++i) if (a[i] == f) return a[i + 1];
    return d;
}
bool isImage(const fs::path& p) {
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return e == ".jpg" || e == ".jpeg" || e == ".png" || e == ".bmp";
}

struct Identity {
    std::string name;
    std::vector<cv::Mat> embeddings;  // one per usable image
};

struct Query {
    cv::Mat  emb;
    int64_t  trueId;     // person id if enrolled, 0 if stranger
    bool     stranger;
};

struct Metrics {
    double thr, margin;
    int correct = 0, wrongName = 0, miss = 0;   // over enrolled positives
    int falseMatch = 0, strangerOk = 0;          // over strangers
    int positives() const { return correct + wrongName + miss; }
    int strangers() const { return falseMatch + strangerOk; }
};

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        std::cerr << "Usage: ffa_eval <dataset_root> [--enroll-per N] [--min-images M] "
                     "[--enrolled K] [--max-strangers S] [--report PATH]\n";
        return 1;
    }
    std::string root      = args[0];
    int enrollPer         = std::stoi(flag(args, "--enroll-per", "3"));
    int minImages         = std::stoi(flag(args, "--min-images", "5"));
    int maxEnrolled       = std::stoi(flag(args, "--enrolled", "25"));
    int maxStrangers      = std::stoi(flag(args, "--max-strangers", "300"));
    int maxPerId          = std::stoi(flag(args, "--max-per-id", "20"));
    std::string reportOut = flag(args, "--report", "");

    if (!fs::is_directory(root)) { std::cerr << "Not a folder: " << root << "\n"; return 1; }

    Config cfg;
    Recognizer rec(cfg);

    // --- 1. Embed the dataset (this is the only expensive part) ---
    std::cout << "Embedding dataset in " << root << " ...\n";
    std::vector<Identity> ids;
    long embMicros = 0; int embCount = 0;

    std::vector<fs::path> people;
    for (const auto& d : fs::directory_iterator(root))
        if (d.is_directory()) people.push_back(d.path());
    std::sort(people.begin(), people.end());

    for (const auto& personDir : people) {
        std::vector<fs::path> imgs;
        for (const auto& f : fs::directory_iterator(personDir))
            if (f.is_regular_file() && isImage(f.path())) imgs.push_back(f.path());
        if ((int)imgs.size() < minImages) continue;
        std::sort(imgs.begin(), imgs.end());
        if ((int)imgs.size() > maxPerId) imgs.resize(maxPerId);  // balance dataset

        Identity id; id.name = personDir.filename().string();
        for (const auto& ip : imgs) {
            cv::Mat img = cv::imread(ip.string());
            std::string reason;
            auto t0 = std::chrono::steady_clock::now();
            cv::Mat emb = bestFaceEmbedding(img, rec, cfg, reason);
            auto t1 = std::chrono::steady_clock::now();
            embMicros += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            ++embCount;
            if (!emb.empty()) id.embeddings.push_back(emb);
        }
        if ((int)id.embeddings.size() >= enrollPer + 1) ids.push_back(std::move(id));
        if ((int)ids.size() >= maxEnrolled + maxStrangers) break;  // enough data
    }
    std::cout << "Usable identities (>= " << (enrollPer + 1) << " good faces): "
              << ids.size() << "\n";
    if ((int)ids.size() < 2) { std::cerr << "Not enough usable identities.\n"; return 1; }

    // --- 2. Split into enrolled vs. stranger, build templates + queries ---
    std::vector<StoredEmbedding> enrolled;   // templates
    std::vector<Person>          persons;
    std::vector<Query>           queries;
    int enrolledCount = 0, strangerCount = 0;

    for (size_t i = 0; i < ids.size(); ++i) {
        bool asEnrolled = enrolledCount < maxEnrolled;
        if (asEnrolled) {
            int64_t pid = ++enrolledCount;
            persons.push_back({pid, ids[i].name, ""});
            for (int k = 0; k < enrollPer; ++k)
                enrolled.push_back({pid, ids[i].embeddings[k]});
            for (size_t k = enrollPer; k < ids[i].embeddings.size(); ++k)
                queries.push_back({ids[i].embeddings[k], pid, false});
        } else {
            if (strangerCount >= maxStrangers) continue;
            ++strangerCount;
            for (const auto& e : ids[i].embeddings)
                queries.push_back({e, 0, true});
        }
    }
    int posCount = 0, negCount = 0;
    for (const auto& q : queries) (q.stranger ? negCount : posCount)++;
    std::cout << "Enrolled identities: " << enrolledCount
              << "  | strangers: " << strangerCount << "\n"
              << "Test queries: " << posCount << " enrolled-positives, "
              << negCount << " stranger-negatives\n\n";

    // --- 3. Sweep threshold x margin ---
    auto evalAt = [&](double thr, double margin) {
        Config c = cfg; c.cosThreshold = (float)thr; c.margin = (float)margin;
        Metrics m; m.thr = thr; m.margin = margin;
        for (const auto& q : queries) {
            MatchResult r = classify(q.emb, enrolled, persons, c);
            bool matched = (r.decision == Decision::Match);
            if (q.stranger) { if (matched) m.falseMatch++; else m.strangerOk++; }
            else if (matched && r.personId == q.trueId) m.correct++;
            else if (matched)                           m.wrongName++;
            else                                        m.miss++;
        }
        return m;
    };
    std::vector<Metrics> grid;
    for (double thr = 0.20; thr <= 0.60001; thr += 0.01)
        for (double margin : {0.0, 0.03, 0.06, 0.10})
            grid.push_back(evalAt(thr, margin));

    // --- 4. Recommend: zero wrong-name & zero false-match first, then max recall ---
    auto better = [](const Metrics& a, const Metrics& b) {
        int badA = a.wrongName + a.falseMatch, badB = b.wrongName + b.falseMatch;
        if (badA != badB) return badA < badB;             // fewer harmful errors
        if (a.correct != b.correct) return a.correct > b.correct;  // more recall
        if (a.thr != b.thr) return a.thr > b.thr;         // more conservative
        return a.margin > b.margin;
    };
    Metrics best = grid.front();
    for (const auto& m : grid) if (better(m, best)) best = m;

    auto pct = [](int n, int d) { return d ? (100.0 * n / d) : 0.0; };
    double avgEmbMs = embCount ? (embMicros / 1000.0 / embCount) : 0.0;

    std::ostringstream rep;
    rep << "# Evaluation Report\n\n";
    rep << "Dataset root: `" << root << "`\n\n";
    rep << "- Enrolled identities: " << enrolledCount
        << " (" << enrollPer << " templates each)\n";
    rep << "- Strangers (held out): " << strangerCount << "\n";
    rep << "- Test queries: " << posCount << " enrolled-positive, "
        << negCount << " stranger-negative\n";
    rep << "- Per-face embedding time (detect+align+embed): **"
        << avgEmbMs << " ms** (avg, CPU)\n";
    rep << "  - Announce latency ~= this x `framesNeeded` (" << cfg.framesNeeded
        << ") + camera time.\n\n";
    rep << "## Recommended operating point\n\n";
    rep << "- **cosThreshold = " << best.thr << "**, **margin = " << best.margin << "**\n\n";
    rep << "| metric | count | rate |\n|---|---|---|\n";
    rep << "| Correct matches (enrolled) | " << best.correct << " | "
        << pct(best.correct, best.positives()) << "% |\n";
    rep << "| **Wrong-name matches** | " << best.wrongName << " | "
        << pct(best.wrongName, best.positives()) << "% |\n";
    rep << "| Missed (unknown/unsure for enrolled) | " << best.miss << " | "
        << pct(best.miss, best.positives()) << "% |\n";
    rep << "| **False matches (strangers)** | " << best.falseMatch << " | "
        << pct(best.falseMatch, best.strangers()) << "% |\n";
    rep << "| Correctly rejected strangers | " << best.strangerOk << " | "
        << pct(best.strangerOk, best.strangers()) << "% |\n";

    rep << "\n## Why this threshold (comparison at margin " << best.margin << ")\n\n";
    rep << "| cosThreshold | correct | wrong-name | miss | false-match |\n";
    rep << "|---|---|---|---|---|\n";
    for (double thr : {0.363, 0.44, 0.50, best.thr}) {
        Metrics m = evalAt(thr, best.margin);
        rep << "| " << thr
            << (thr == 0.363 ? " (OpenCV default)" : "")
            << (thr == best.thr ? " (**chosen**)" : "")
            << " | " << pct(m.correct, m.positives()) << "% | "
            << m.wrongName << " | " << pct(m.miss, m.positives()) << "% | "
            << m.falseMatch << " |\n";
    }

    std::cout << rep.str();
    std::cout << "\n(Full sweep: " << grid.size() << " threshold/margin combos evaluated.)\n";

    if (!reportOut.empty()) {
        std::ofstream f(reportOut);
        f << rep.str();
        std::cout << "\nReport written to " << reportOut << "\n";
    }
    return 0;
}
