// ffa_server — local-only HTTP API + MJPEG preview for the Familiar Face
// Assistant. Bound to 127.0.0.1 ONLY. The C++ side owns the camera and the
// recognition pipeline; the React UI is pure presentation and talks to this
// server over localhost. No data ever leaves the machine.
//
// One background thread owns the camera and the Recognizer (which is not
// thread-safe). HTTP handlers communicate with it through shared state guarded
// by mutexes. All SQLite access is serialized by dbMutex.
//
// Endpoints (all JSON unless noted):
//   GET  /api/status            current mode, camera state, last result, announce event
//   GET  /api/people            enrolled people + embedding counts
//   POST /api/enroll/start      {name, reminder}  begin enrollment
//   POST /api/enroll/capture    grab current frame -> store one embedding
//   POST /api/enroll/finish     end enrollment
//   POST /api/watch/start       begin live recognition
//   POST /api/watch/stop        stop
//   POST /api/mute              {muted:bool}
//   POST /api/people/delete     {name}            delete one person
//   POST /api/people/deleteAll  wipe all
//   GET  /stream.mjpg           multipart MJPEG live preview
//   (static UI served from ../../ui/dist when built)
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>

#include "ffa/config.hpp"
#include "ffa/camera.hpp"
#include "ffa/recognizer.hpp"
#include "ffa/storage.hpp"
#include "ffa/safety.hpp"
#include "ffa/pipeline.hpp"

using json = nlohmann::json;
using namespace ffa;
using namespace std::chrono_literals;

namespace {

enum class Mode { Idle, Watch, Enroll };

struct Shared {
    std::mutex m;
    Mode mode = Mode::Idle;
    bool cameraOpen = false;
    bool muted = false;

    std::vector<uchar> jpeg;      // latest annotated preview frame (JPEG bytes)

    // enrollment
    std::string enrollName;
    int64_t     enrollPid = 0;
    int         enrollCount = 0;
    bool        captureReady = false;   // current frame has a good face
    std::string captureReason = "";
    bool        captureRequested = false;
    long        captureSeq = 0;         // bumps when a capture attempt finishes
    bool        captureOk = false;
    std::string captureMsg = "";

    // live recognition result (for the UI to display)
    std::string decision = "idle";      // match|unknown|unsure|lowquality|idle
    std::string name = "";
    std::string reminder = "";
    double      score = 0.0;

    // announcement event — browser speaks when announceId increases
    long        announceId = 0;
    std::string announceName = "";
    std::string announceReminder = "";
};

Shared g;
std::mutex dbMutex;  // serializes ALL SQLite access

// Draw a gray placeholder frame with a message (when the camera is unavailable).
std::vector<uchar> placeholder(const std::string& msg) {
    cv::Mat img(480, 640, CV_8UC3, cv::Scalar(40, 40, 40));
    cv::putText(img, msg, {30, 240}, cv::FONT_HERSHEY_SIMPLEX, 0.8,
                {200, 200, 200}, 2);
    std::vector<uchar> buf;
    cv::imencode(".jpg", img, buf);
    return buf;
}

// The camera + recognition thread. Owns Camera, Recognizer and Storage writes.
void cameraLoop(std::atomic<bool>& running, Config cfg) {
    Recognizer rec(cfg);
    Storage    store(cfg.dbPath);

    // enrolled set, (re)loaded when watch starts
    std::vector<StoredEmbedding> enrolled;
    std::vector<Person>          people;
    FrameVoter voter(cfg.framesNeeded);
    std::unordered_map<int64_t, std::chrono::steady_clock::time_point> lastAnnounce;
    Mode lastMode = Mode::Idle;

    Camera cam(0);
    { std::lock_guard<std::mutex> lk(g.m); g.cameraOpen = cam.isOpen(); }

    cv::Mat frame;
    while (running) {
        if (!cam.isOpen()) {
            { std::lock_guard<std::mutex> lk(g.m);
              g.cameraOpen = false; g.jpeg = placeholder("Camera unavailable"); }
            std::this_thread::sleep_for(500ms);
            cam = Camera(0);  // retry
            { std::lock_guard<std::mutex> lk(g.m); g.cameraOpen = cam.isOpen(); }
            continue;
        }
        if (!cam.read(frame)) { std::this_thread::sleep_for(30ms); continue; }

        Mode mode;
        bool wantCapture; int64_t enrollPid;
        { std::lock_guard<std::mutex> lk(g.m);
          mode = g.mode; wantCapture = g.captureRequested; enrollPid = g.enrollPid; }

        // Reload enrolled set when entering Watch.
        if (mode == Mode::Watch && lastMode != Mode::Watch) {
            std::lock_guard<std::mutex> lk(dbMutex);
            people = store.listPeople();
            enrolled = store.allEmbeddings();
            voter.reset();
        }
        lastMode = mode;

        cv::Mat view = frame.clone();
        std::string label = "idle";
        cv::Scalar col(200, 200, 200);

        // Find the largest face for enroll/watch feedback.
        auto faces = rec.detect(frame);
        int fi = -1, area = 0;
        for (size_t i = 0; i < faces.size(); ++i)
            if (faces[i].box.area() > area) { area = faces[i].box.area(); fi = int(i); }

        if (mode == Mode::Enroll) {
            std::string reason = "no face"; bool ready = false;
            if (fi >= 0) {
                ready = passesQualityGate(frame, faces[fi], cfg, reason);
                cv::rectangle(view, faces[fi].box,
                              ready ? cv::Scalar(0, 200, 0) : cv::Scalar(0, 165, 255), 2);
            }
            label = ready ? "ready - capture" : reason;
            col = ready ? cv::Scalar(0, 200, 0) : cv::Scalar(0, 165, 255);

            if (wantCapture) {
                bool ok = false; std::string msg = reason;
                if (fi >= 0 && ready) {
                    cv::Mat emb = rec.embed(frame, faces[fi]);
                    { std::lock_guard<std::mutex> lk(dbMutex); store.addEmbedding(enrollPid, emb); }
                    ok = true; msg = "ok";
                }
                std::lock_guard<std::mutex> lk(g.m);
                if (ok) g.enrollCount++;
                g.captureOk = ok; g.captureMsg = msg;
                g.captureRequested = false; g.captureSeq++;
            }
            std::lock_guard<std::mutex> lk(g.m);
            g.captureReady = ready; g.captureReason = reason;
        }
        else if (mode == Mode::Watch) {
            std::string decision = "unknown", nm = "", rem = ""; double sc = 0;
            col = cv::Scalar(0, 0, 255);
            if (fi >= 0) {
                std::string reason;
                if (passesQualityGate(frame, faces[fi], cfg, reason)) {
                    cv::Mat q = rec.embed(frame, faces[fi]);
                    MatchResult r = classify(q, enrolled, people, cfg);
                    sc = r.score;
                    if (r.decision == Decision::Match) {
                        decision = "match"; nm = r.name; rem = r.reminder;
                        col = cv::Scalar(0, 200, 0);
                        if (voter.accept(r)) {
                            auto now = std::chrono::steady_clock::now();
                            auto it = lastAnnounce.find(r.personId);
                            bool cooled = it == lastAnnounce.end() ||
                                std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count()
                                    >= cfg.cooldownSeconds;
                            if (cooled) {
                                lastAnnounce[r.personId] = now;
                                std::lock_guard<std::mutex> lk(g.m);
                                g.announceId++; g.announceName = nm; g.announceReminder = rem;
                            }
                        }
                    } else if (r.decision == Decision::Unsure) {
                        decision = "unsure"; col = cv::Scalar(0, 165, 255); voter.reset();
                    } else { decision = "unknown"; voter.reset(); }
                } else { decision = "lowquality"; col = cv::Scalar(0, 165, 255); voter.reset(); }
                cv::rectangle(view, faces[fi].box, col, 2);
            } else { voter.reset(); }
            label = decision + (nm.empty() ? "" : ": " + nm);
            std::lock_guard<std::mutex> lk(g.m);
            g.decision = decision; g.name = nm; g.reminder = rem; g.score = sc;
        }
        else {  // Idle
            if (fi >= 0) cv::rectangle(view, faces[fi].box, cv::Scalar(180,180,180), 1);
            std::lock_guard<std::mutex> lk(g.m); g.decision = "idle";
        }

        // Overlay label + camera indicator, encode JPEG.
        cv::putText(view, label, {20, 40}, cv::FONT_HERSHEY_SIMPLEX, 0.9, col, 2);
        cv::circle(view, {view.cols - 30, 30}, 10, {0, 0, 255}, -1);
        cv::putText(view, "REC", {view.cols - 80, 36}, cv::FONT_HERSHEY_SIMPLEX,
                    0.6, {0, 0, 255}, 2);
        std::vector<uchar> buf;
        cv::imencode(".jpg", view, buf);
        { std::lock_guard<std::mutex> lk(g.m); g.jpeg = std::move(buf); g.cameraOpen = true; }
    }
}

json peopleJson(Storage& store) {
    std::lock_guard<std::mutex> lk(dbMutex);
    auto ppl = store.listPeople();
    auto embs = store.allEmbeddings();
    json arr = json::array();
    for (const auto& p : ppl) {
        int n = 0; for (const auto& e : embs) if (e.personId == p.id) ++n;
        arr.push_back({{"id", p.id}, {"name", p.name},
                       {"reminder", p.reminder}, {"embeddings", n}});
    }
    return arr;
}

}  // namespace

int main(int argc, char** argv) {
    Config cfg;
    int port = 8765;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--port") port = std::stoi(argv[i + 1]);

    // macOS: the camera authorization request must originate from the main
    // thread (AVFoundation main run loop). Briefly open/release the device here
    // so the permission prompt appears correctly on first run; the background
    // thread then owns the camera for real once access is granted.
    { cv::VideoCapture prime(0); if (prime.isOpened()) prime.release(); }

    std::atomic<bool> running{true};
    std::thread camThread(cameraLoop, std::ref(running), cfg);

    // A dedicated Storage for read/delete handlers (guarded by dbMutex).
    Storage apiStore(cfg.dbPath);

    httplib::Server svr;

    // Permissive CORS for local dev (Vite on another port). Localhost only.
    svr.set_default_headers({{"Access-Control-Allow-Origin", "*"},
                             {"Access-Control-Allow-Headers", "Content-Type"},
                             {"Access-Control-Allow-Methods", "GET,POST,OPTIONS"}});
    svr.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    svr.Get("/api/status", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(g.m);
        const char* modeStr = g.mode == Mode::Watch ? "watch"
                            : g.mode == Mode::Enroll ? "enroll" : "idle";
        json j = {
            {"mode", modeStr}, {"cameraOpen", g.cameraOpen}, {"muted", g.muted},
            {"decision", g.decision}, {"name", g.name}, {"reminder", g.reminder},
            {"score", g.score},
            {"enroll", {{"name", g.enrollName}, {"count", g.enrollCount},
                        {"ready", g.captureReady}, {"reason", g.captureReason}}},
            {"announce", {{"id", g.announceId}, {"name", g.announceName},
                          {"reminder", g.announceReminder}}}
        };
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/people", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(peopleJson(apiStore).dump(), "application/json");
    });

    svr.Post("/api/enroll/start", [&](const httplib::Request& req, httplib::Response& res) {
        auto body = json::parse(req.body, nullptr, false);
        std::string name = body.value("name", "");
        std::string reminder = body.value("reminder", "");
        if (name.empty()) { res.status = 400; res.set_content("{\"error\":\"name required\"}", "application/json"); return; }
        int64_t pid;
        { std::lock_guard<std::mutex> lk(dbMutex); pid = apiStore.upsertPerson(name, reminder); }
        std::lock_guard<std::mutex> lk(g.m);
        g.mode = Mode::Enroll; g.enrollName = name; g.enrollPid = pid;
        g.enrollCount = 0; g.captureRequested = false;
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/api/enroll/capture", [&](const httplib::Request&, httplib::Response& res) {
        long seq;
        { std::lock_guard<std::mutex> lk(g.m);
          if (g.mode != Mode::Enroll) { res.status = 409;
            res.set_content("{\"error\":\"not enrolling\"}", "application/json"); return; }
          seq = g.captureSeq; g.captureRequested = true; }
        // wait for the camera thread to process this capture (up to ~2s)
        for (int i = 0; i < 400; ++i) {
            std::this_thread::sleep_for(5ms);
            std::lock_guard<std::mutex> lk(g.m);
            if (g.captureSeq != seq) {
                json j = {{"ok", g.captureOk}, {"reason", g.captureMsg}, {"count", g.enrollCount}};
                res.set_content(j.dump(), "application/json"); return;
            }
        }
        res.status = 504; res.set_content("{\"error\":\"capture timeout\"}", "application/json");
    });

    svr.Post("/api/enroll/finish", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(g.m);
        g.mode = Mode::Idle;
        json j = {{"ok", true}, {"count", g.enrollCount}};
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/watch/start", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(g.m); g.mode = Mode::Watch;
        res.set_content("{\"ok\":true}", "application/json");
    });
    svr.Post("/api/watch/stop", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(g.m); g.mode = Mode::Idle; g.decision = "idle";
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/api/mute", [&](const httplib::Request& req, httplib::Response& res) {
        auto body = json::parse(req.body, nullptr, false);
        std::lock_guard<std::mutex> lk(g.m); g.muted = body.value("muted", false);
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/api/people/delete", [&](const httplib::Request& req, httplib::Response& res) {
        auto body = json::parse(req.body, nullptr, false);
        std::string name = body.value("name", "");
        bool ok; { std::lock_guard<std::mutex> lk(dbMutex); ok = apiStore.deletePersonByName(name); }
        json j = {{"ok", ok}};
        res.set_content(j.dump(), "application/json");
    });
    svr.Post("/api/people/deleteAll", [&](const httplib::Request&, httplib::Response& res) {
        { std::lock_guard<std::mutex> lk(dbMutex); apiStore.deleteAll(); }
        res.set_content("{\"ok\":true}", "application/json");
    });

    // MJPEG live preview.
    svr.Get("/stream.mjpg", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [&](size_t, httplib::DataSink& sink) {
                std::vector<uchar> buf;
                { std::lock_guard<std::mutex> lk(g.m); buf = g.jpeg; }
                if (!buf.empty()) {
                    std::string head = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: "
                                     + std::to_string(buf.size()) + "\r\n\r\n";
                    sink.write(head.data(), head.size());
                    sink.write(reinterpret_cast<const char*>(buf.data()), buf.size());
                    sink.write("\r\n", 2);
                }
                std::this_thread::sleep_for(33ms);
                return true;  // keep streaming
            });
    });

    // Serve the built React UI if present.
    if (!svr.set_mount_point("/", "../../ui/dist"))
        std::cout << "(UI not built yet; run `npm run build` in ui/. API still works.)\n";

    std::cout << "Familiar Face Assistant server on http://127.0.0.1:" << port << "\n"
              << "Open the UI in a browser. Ctrl-C to stop.\n";
    svr.listen("127.0.0.1", port);

    running = false;
    camThread.join();
    return 0;
}
