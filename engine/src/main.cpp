// Familiar Face Assistant — Milestone 1 command-line prototype.
//
// Subcommands:
//   ffa enroll "Name" [--reminder "cousin"] [--shots N]   capture & store a person
//   ffa list                                              show enrolled people
//   ffa delete "Name"                                     delete one person
//   ffa delete --all                                      wipe all enrolled data
//   ffa watch                                             continuous live recognition
//
// This is a local-only prototype. See docs/PRIVACY.md and docs/ARCHITECTURE.md.
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include "ffa/config.hpp"
#include "ffa/camera.hpp"
#include "ffa/recognizer.hpp"
#include "ffa/storage.hpp"
#include "ffa/safety.hpp"
#include "ffa/announcer.hpp"

using namespace ffa;

namespace {

void usage() {
    std::cout <<
        "Familiar Face Assistant (local-only prototype)\n"
        "Usage:\n"
        "  ffa enroll \"Name\" [--reminder \"cousin\"] [--shots N]\n"
        "  ffa list\n"
        "  ffa delete \"Name\"\n"
        "  ffa delete --all\n"
        "  ffa watch\n";
}

// Pick the largest detected face (the person the user is facing).
int largestFace(const std::vector<DetectedFace>& faces) {
    int idx = -1; int area = 0;
    for (size_t i = 0; i < faces.size(); ++i) {
        int a = faces[i].box.area();
        if (a > area) { area = a; idx = static_cast<int>(i); }
    }
    return idx;
}

std::string getFlag(const std::vector<std::string>& args, const std::string& flag,
                    const std::string& def) {
    for (size_t i = 0; i + 1 < args.size(); ++i)
        if (args[i] == flag) return args[i + 1];
    return def;
}

int cmdEnroll(const Config& cfg, const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << "enroll needs a name\n"; return 1; }
    std::string name     = args[0];
    std::string reminder = getFlag(args, "--reminder", "");
    int shots            = std::stoi(getFlag(args, "--shots", std::to_string(cfg.enrollShots)));

    Recognizer rec(cfg);
    Storage    store(cfg.dbPath);
    Camera     cam(0);
    if (!cam.isOpen()) { std::cerr << "Cannot open camera.\n"; return 1; }

    std::cout << "Enrolling \"" << name << "\". Look at the camera.\n"
              << "Press SPACE to capture a good shot (" << shots << " needed), "
              << "vary your angle slightly each time. Press q to cancel.\n";

    int64_t pid = store.upsertPerson(name, reminder);
    int captured = 0;
    cv::Mat frame;
    while (captured < shots) {
        if (!cam.read(frame)) break;
        auto faces = rec.detect(frame);
        int fi = largestFace(faces);

        cv::Mat view = frame.clone();
        std::string hint = "No face";
        cv::Scalar col(0, 0, 255);
        if (fi >= 0) {
            std::string reason;
            bool ok = passesQualityGate(frame, faces[fi], cfg, reason);
            col  = ok ? cv::Scalar(0, 200, 0) : cv::Scalar(0, 165, 255);
            hint = ok ? "OK - press SPACE" : reason;
            cv::rectangle(view, faces[fi].box, col, 2);
        }
        cv::putText(view, hint, {20, 40}, cv::FONT_HERSHEY_SIMPLEX, 0.8, col, 2);
        cv::putText(view, "captured " + std::to_string(captured) + "/" + std::to_string(shots),
                    {20, 75}, cv::FONT_HERSHEY_SIMPLEX, 0.7, {255, 255, 255}, 2);
        cv::imshow("Enroll (camera active)", view);

        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 27) { std::cout << "Cancelled.\n"; break; }
        if (key == ' ' && fi >= 0) {
            std::string reason;
            if (passesQualityGate(frame, faces[fi], cfg, reason)) {
                store.addEmbedding(pid, rec.embed(frame, faces[fi]));
                ++captured;
                std::cout << "  captured " << captured << "/" << shots << "\n";
            } else {
                std::cout << "  skipped (" << reason << ")\n";
            }
        }
    }
    cv::destroyAllWindows();
    std::cout << (captured >= shots ? "Enrollment complete.\n" : "Enrollment incomplete.\n");
    return 0;
}

int cmdList(const Config& cfg) {
    Storage store(cfg.dbPath);
    auto people = store.listPeople();
    auto embs   = store.allEmbeddings();
    if (people.empty()) { std::cout << "No one enrolled yet.\n"; return 0; }
    std::cout << "Enrolled people:\n";
    for (const auto& p : people) {
        int n = 0;
        for (const auto& e : embs) if (e.personId == p.id) ++n;
        std::cout << "  - " << p.name;
        if (!p.reminder.empty()) std::cout << " (" << p.reminder << ")";
        std::cout << "  [" << n << " embeddings]\n";
    }
    return 0;
}

int cmdDelete(const Config& cfg, const std::vector<std::string>& args) {
    Storage store(cfg.dbPath);
    if (!args.empty() && args[0] == "--all") {
        store.deleteAll();
        std::cout << "All enrolled data deleted.\n";
        return 0;
    }
    if (args.empty()) { std::cerr << "delete needs a name or --all\n"; return 1; }
    bool ok = store.deletePersonByName(args[0]);
    std::cout << (ok ? "Deleted \"" + args[0] + "\".\n" : "No such person.\n");
    return ok ? 0 : 1;
}

int cmdWatch(const Config& cfg) {
    Recognizer rec(cfg);
    Storage    store(cfg.dbPath);
    Announcer  announcer(cfg);
    FrameVoter voter(cfg.framesNeeded);

    auto people = store.listPeople();
    auto embs   = store.allEmbeddings();
    if (embs.empty()) { std::cout << "No one enrolled — run `ffa enroll` first.\n"; return 0; }

    Camera cam(0);
    if (!cam.isOpen()) { std::cerr << "Cannot open camera.\n"; return 1; }

    std::cout << "Watching (hands-free). It announces an enrolled person once, "
                 "then stays quiet. Press q to stop.\n";

    cv::Mat frame;
    while (true) {
        if (!cam.read(frame)) break;
        auto faces = rec.detect(frame);
        int fi = largestFace(faces);

        cv::Mat view = frame.clone();
        std::string label = "watching...";
        cv::Scalar col(200, 200, 200);

        if (fi >= 0) {
            std::string reason;
            if (passesQualityGate(frame, faces[fi], cfg, reason)) {
                cv::Mat q = rec.embed(frame, faces[fi]);
                MatchResult r = classify(q, embs, people, rec, cfg);
                switch (r.decision) {
                    case Decision::Match:
                        label = r.name + (r.reminder.empty() ? "" : " (" + r.reminder + ")");
                        col = {0, 200, 0};
                        if (voter.accept(r))
                            announcer.announce(r.personId, r.name, r.reminder);
                        break;
                    case Decision::Unsure:
                        label = "unsure"; col = {0, 165, 255}; voter.reset(); break;
                    case Decision::Unknown:
                        label = "unknown"; col = {0, 0, 255}; voter.reset(); break;
                    default:
                        label = "low quality"; col = {0, 165, 255}; voter.reset(); break;
                }
            } else {
                label = reason; col = {0, 165, 255}; voter.reset();
            }
            cv::rectangle(view, faces[fi].box, col, 2);
        } else {
            voter.reset();
        }

        cv::putText(view, label, {20, 40}, cv::FONT_HERSHEY_SIMPLEX, 0.9, col, 2);
        cv::putText(view, "camera active", {20, view.rows - 20},
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 0, 255}, 2);
        cv::imshow("Watch (camera active)", view);
        if ((cv::waitKey(1) & 0xFF) == 'q') break;
    }
    cv::destroyAllWindows();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Config cfg;
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) { usage(); return 1; }

    std::string cmd = args[0];
    std::vector<std::string> rest(args.begin() + 1, args.end());
    try {
        if (cmd == "enroll") return cmdEnroll(cfg, rest);
        if (cmd == "list")   return cmdList(cfg);
        if (cmd == "delete") return cmdDelete(cfg, rest);
        if (cmd == "watch")  return cmdWatch(cfg);
        usage();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}
