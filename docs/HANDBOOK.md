# Project Handbook — Familiar Face Assistant

> **Read this to understand the whole project.** It is written for someone who
> does not already know these technologies. It goes from "what is this" down to
> "what every file does" and "how to explain it in an interview."
>
> **Maintenance:** this file is kept up to date as the project grows. Last
> updated for: **Milestone 4 in progress** — the web UI (C++ HTTP server +
> React/TypeScript front-end) is built on top of the tuned, tested engine from
> Milestones 1–3.

---

## Table of contents

1. [What this project is](#1-what-this-project-is)
2. [Glossary — every term & technology in plain English](#2-glossary)
3. [The big picture — how a face becomes a name](#3-the-big-picture)
4. [Repository layout](#4-repository-layout)
5. [File-by-file reference](#5-file-by-file-reference)
6. [The recognition pipeline in detail](#6-the-recognition-pipeline-in-detail)
7. [The safety logic in detail](#7-the-safety-logic-in-detail)
8. [How data is stored — and how to see it](#8-how-data-is-stored-and-how-to-see-it)
9. [How the AI "learns" a person (enrollment vs training)](#9-how-the-ai-learns-a-person)
10. [Build, run, test, evaluate — every command](#10-build-run-test-evaluate)
11. [The evaluation results](#11-the-evaluation-results)
12. [Interview prep](#12-interview-prep)
13. [Roadmap](#13-roadmap)

---

## 1. What this project is

A **private, local-only assistant** that helps a person with **prosopagnosia**
(face blindness) recognize people they know. A camera sees a face, the software
compares it to a small set of people the user has **enrolled** (with consent),
and it discreetly says the name — e.g. *"Possible match: Sarah."* If it isn't
sure, it stays silent or says "unknown." It never guesses.

**Key principles baked into the design:**
- **Local only** — no cloud, no internet at runtime, no accounts. Face data
  never leaves the machine.
- **Consent only** — it only recognizes people who agreed to be enrolled. It
  cannot identify strangers.
- **Conservative** — a *wrong* name is socially harmful, so the system prefers
  silence over a risky guess.

**Current form:** a C++ command-line program. A web-style UI comes later.

---

## 2. Glossary

Plain-English definitions of everything used in this project.

| Term | What it means (simple) |
|---|---|
| **C++** | The programming language the engine is written in. Fast, low-level, compiled (turned into machine code before running). |
| **OpenCV** | A famous open-source library for computer vision (working with images/video). We use it for the camera, finding faces, and running the AI models. "CV" = Computer Vision. |
| **Computer vision** | Getting a computer to understand images — e.g. "there's a face here." |
| **Neural network / CNN** | The "AI" — a math model loosely inspired by the brain, trained on lots of data. **CNN** (Convolutional Neural Network) is the type used for images. |
| **Model** | A trained neural network saved to a file (here, `.onnx` files). Running it on new input is called **inference**. |
| **ONNX** | A standard file format for saving/sharing trained models. Our two models are `.onnx` files. |
| **YuNet** | The specific model that **detects** faces (finds where they are). |
| **SFace** | The specific model that turns a face into an **embedding** (the 128-number fingerprint). |
| **Embedding** | A list of numbers (here, **128** of them) that represents a face's identity. Same person → similar numbers; different people → different numbers. Also called a "feature vector" or "face template." |
| **Cosine similarity** | A simple math formula that measures how similar two embeddings are, from -1 (opposite) to 1 (identical). It's the *angle* between two lists of numbers. |
| **Threshold** | A cut-off. If similarity is above it → possible match; below → unknown. Ours is tuned to **0.54**. |
| **Margin** | How much the best match must beat the *second-best* match. Stops the app confusing two similar-looking people. Ours is **0.10**. |
| **SQLite** | A tiny database that lives in a single file on disk. Stores names + embeddings. |
| **CMake** | A tool that organizes and runs the C++ build (turning source code into a runnable program). |
| **Homebrew** | The macOS package manager we used to install OpenCV, CMake, etc. (`brew install ...`). |
| **GoogleTest** | A library for writing automated **unit tests** (small checks that a piece of code works). |
| **Inference** | Running a trained model on new data to get a result (vs. *training*, which creates the model). |
| **Enrollment** | Adding a person: taking their photos, making embeddings, and saving them. This is how the app "learns" someone — see §9. |
| **TTS** | Text-to-speech (the spoken "Possible match: Sarah"). We use macOS's built-in `say`. |
| **Localhost / 127.0.0.1** | Your own computer talking to itself. The web UI uses this so nothing goes over the network. |
| **HTTP server / API** | A program that answers requests (like a mini website). Ours (`ffa_server`) lets the UI ask things like "who's enrolled?" or "start watching." |
| **REST / endpoint** | A style of API where each URL (an "endpoint," e.g. `/api/people`) does one thing. |
| **JSON** | A simple text format for data (`{"name":"Sarah"}`). The UI and server talk in JSON. |
| **cpp-httplib** | The C++ library that turns our engine into an HTTP server. Header-only (just one file to include). |
| **nlohmann/json** | The C++ library for reading/writing JSON. |
| **MJPEG** | "Motion JPEG" — a way to stream video as a rapid series of JPEG images. The server streams the camera preview to the browser this way. |
| **React** | A popular JavaScript/TypeScript library for building user interfaces out of reusable "components." |
| **TypeScript** | JavaScript with type-checking (catches mistakes before running). The whole UI is TypeScript. |
| **Vite** | The build tool that compiles/bundles the React app and runs a fast dev server. |
| **Component** | A reusable piece of UI in React (e.g. `EnrollPanel`) — its own file, own logic. |
| **thread / mutex** | A *thread* lets code run at the same time as other code (the camera loop runs in its own thread). A *mutex* is a lock that stops two threads from touching the same data at once. |
| **Web Speech API** | A browser feature that speaks text aloud. The UI uses it for the spoken "Possible match." |

---

## 3. The big picture

The system is an **assembly line**. An image goes in; a decision comes out.

```
 Camera frame
     │
     ▼
 [1] Detect faces        ← AI model #1 (YuNet)      "where are the faces?"
     │
     ▼
 [2] Quality gate         ← plain rules             "is this face good enough?"
     │        (reject tiny / blurry / off-screen)
     ▼
 [3] Make embedding       ← AI model #2 (SFace)     "turn the face into 128 numbers"
     │
     ▼
 [4] Compare to enrolled  ← plain math (cosine)     "who does this look like?"
     │
     ▼
 [5] Decide               ← plain rules             threshold + margin
     │                                              → match / unknown / unsure
     ▼
 [6] Multi-frame vote     ← plain rules             "did 5 frames in a row agree?"
     │
     ▼
 [7] Announce once        ← Announcer               speak the name, then cooldown
```

**The single most important idea:** steps 1 and 3 are AI (pretrained neural
networks we just run). Steps 2, 4, 5, 6 are **plain, readable logic that we
control** — that's where the "don't say a wrong name" safety lives. The
safety-critical decision is *not* a black box.

### How the web app is wired (Milestone 4)

The engine now also runs as a local server, with a browser UI on top. Nothing
leaves the machine — it's your computer talking to itself on `127.0.0.1`:

```
  Browser (React + TypeScript UI)
     │  asks JSON questions:  /api/people, /api/watch/start, /api/enroll/capture ...
     │  shows live video:     <img src="/stream.mjpg">
     │  speaks the name:      Web Speech API (in the browser)
     ▼
  ffa_server (C++, cpp-httplib)  ── 127.0.0.1 only ──┐
     │                                               │
     │  one background thread owns the camera +      │
     │  runs the recognition pipeline (§3 above)     │
     ▼                                               │
  ffacore (the same engine library from M1–3) ───────┘
```

- The **server** owns the camera and does all recognition (the private part).
- The **browser** only shows things and plays audio (the presentation part).
- This split is exactly why a future phone/wearable client is possible: it would
  talk to the same API.

---

## 4. Repository layout

```
Smart-Glasses/
├── README.md                 ← quick start
├── LICENSE                   ← MIT license
├── docs/
│   ├── HANDBOOK.md           ← THIS FILE (learn the whole project)
│   ├── ARCHITECTURE.md       ← shorter architecture overview
│   ├── PRIVACY.md            ← privacy & safety rules
│   ├── LICENSES.md           ← dependency & model licenses
│   └── EVALUATION.md         ← measured accuracy results
├── engine/                   ← the C++ program
│   ├── CMakeLists.txt        ← build instructions
│   ├── include/ffa/          ← headers (.hpp) — the "table of contents" of each module
│   │   ├── config.hpp        ← ALL the tunable settings
│   │   ├── camera.hpp
│   │   ├── recognizer.hpp
│   │   ├── safety.hpp
│   │   ├── storage.hpp
│   │   ├── announcer.hpp
│   │   └── pipeline.hpp
│   ├── src/                  ← implementations (.cpp) — the actual code
│   │   ├── main.cpp          ← the CLI commands
│   │   ├── camera.cpp
│   │   ├── recognizer.cpp    ← the AI (runs both models)
│   │   ├── safety.cpp        ← the decision logic
│   │   ├── storage.cpp       ← the database
│   │   ├── announcer.cpp
│   │   ├── pipeline.cpp      ← shared "image → best face embedding" helper
│   │   ├── eval.cpp          ← the accuracy/tuning tool (ffa_eval)
│   │   └── server.cpp        ← the local HTTP server + MJPEG preview (ffa_server)
│   ├── tests/
│   │   └── test_safety.cpp   ← 15 unit tests for the decision logic
│   ├── models/               ← the .onnx model files (downloaded, not in git)
│   └── build/                ← compiled output (created by CMake, not in git)
├── ui/                       ← the React + TypeScript web front-end
│   ├── package.json          ← UI dependencies + scripts
│   ├── vite.config.ts        ← build config + dev proxy to the C++ server
│   ├── index.html            ← the page shell
│   ├── src/
│   │   ├── main.tsx          ← React entry point
│   │   ├── App.tsx           ← top-level UI: polls status, layout, speech
│   │   ├── api.ts            ← typed client for the server's API
│   │   ├── styles.css        ← all styling
│   │   └── components/
│   │       ├── Preview.tsx      ← live camera preview (<img> MJPEG)
│   │       ├── WatchPanel.tsx   ← start/stop watching + result card
│   │       ├── EnrollPanel.tsx  ← enrollment flow
│   │       └── PeoplePanel.tsx  ← review & delete enrolled people
│   └── dist/                 ← built UI (created by `npm run build`, not in git)
├── scripts/
│   ├── download_models.sh    ← downloads YuNet + SFace models
│   └── run.sh                ← build everything + start the web app
└── data/                     ← the local database lives here (not in git)
    └── faces.db              ← SQLite: names + embeddings
```

> **Headers (`.hpp`) vs. sources (`.cpp`):** in C++, the `.hpp` file *declares*
> what a module offers (like a menu), and the `.cpp` file *implements* it (the
> kitchen). To learn what a module does, read its `.hpp` first.

---

## 5. File-by-file reference

### `engine/include/ffa/config.hpp` — the control panel
**Everything you can tune, in one place.** If a number matters, it's here.

| Setting | Value | Meaning |
|---|---|---|
| `detScoreThreshold` | 0.90 | how confident the detector must be that it's a face |
| `minFacePx` | 80 | reject faces smaller than 80px (too far away) |
| `minBlurVar` | 40.0 | reject blurry faces (measured by "Laplacian variance") |
| `cosThreshold` | **0.54** | how similar a face must be to a saved one to match |
| `margin` | **0.10** | how much the top match must beat the runner-up |
| `framesNeeded` | 5 | how many frames in a row must agree before speaking |
| `cooldownSeconds` | 120 | don't repeat a person's name for 2 minutes |
| `enrollShots` | 5 | how many photos to capture when enrolling |
| `dbPath`, `detectorModel`, `recognizerModel` | paths | where the database and model files are |

### `engine/src/recognizer.cpp` (+ `recognizer.hpp`) — the AI
This is the **only file that touches the neural networks.**
- `Recognizer::Recognizer(cfg)` — loads the two `.onnx` models into memory.
- `Recognizer::detect(frame)` — runs **YuNet**, returns a list of `DetectedFace`
  (each has a bounding `box`, a `detScore`, and `detRow` the raw detector data).
- `Recognizer::embed(frame, face)` — aligns/crops the face and runs **SFace** to
  produce the **128-number embedding**.
- `cosine(a,b)` — (kept for reference) the model's own similarity function.

### `engine/src/safety.cpp` (+ `safety.hpp`) — the brain's judgment ⭐
**The most important file for interviews.** All the "should we say it?" logic.
- `cosineSim(a,b)` — pure-math cosine similarity (no model needed → testable).
- `passesQualityGate(frame, face, cfg, reason)` — the quality checks; `reason`
  says *why* a face was rejected.
- `classify(query, enrolled, people, cfg)` — compares one face to all enrolled
  people and returns a `MatchResult` with a `Decision`:
  - `Match` (confident + unambiguous), `Unknown` (too different),
    `Unsure` (close but ambiguous), `LowQuality`.
- `FrameVoter` — a small class that only fires after the **same** person wins
  `framesNeeded` frames in a row (`accept()` / `reset()`).

### `engine/src/storage.cpp` (+ `storage.hpp`) — the memory
Talks to the SQLite database.
- `upsertPerson(name, reminder)` — add/update a person, return their id.
- `addEmbedding(personId, vec)` — save one 128-number embedding.
- `listPeople()` / `allEmbeddings()` — read them back.
- `deletePersonByName(name)` / `deleteAll()` — deletion that really deletes.
- Two tables: `people` and `embeddings` (schema in §8).

### `engine/src/camera.cpp` (+ `camera.hpp`) — the eyes
A thin wrapper over OpenCV's webcam access (`read(frame)`, `isOpen()`).
**Why it's separate:** for the future wearable, only this file changes (swap the
webcam for a phone/glasses camera stream) — nothing else.

### `engine/src/announcer.cpp` (+ `announcer.hpp`) — the mouth
- `announce(personId, name, reminder)` — prints + speaks (macOS `say`) the name,
  but **only if** that person hasn't been announced in the last `cooldownSeconds`.
  Keeps a map of "who was last announced when." This is what prevents spam.

### `engine/src/pipeline.cpp` (+ `pipeline.hpp`) — shared glue
- `bestFaceEmbedding(frame, rec, cfg, reason)` — does "find the biggest face →
  quality-check it → embed it" in one call. Used by the camera path, the offline
  image commands, and the evaluation tool so they all behave identically.

### `engine/src/main.cpp` — the controls
Defines the command-line commands and wires the modules together:
- `enroll` (webcam), `enroll-dir` (from an image folder), `identify` (one image),
  `list`, `delete` / `--all`, `watch` (live recognition).
- Helpers: `largestFace()`, `getFlag()`, `isImage()`, and one `cmd*` function per
  command.

### `engine/src/eval.cpp` — the measuring tape
A separate program (`ffa_eval`) that runs the real pipeline over a labeled image
dataset, sweeps threshold/margin combinations, and reports accuracy metrics. This
is how we found `0.54 / 0.10`. See §11.

### `engine/src/server.cpp` — the local web server ⭐ (the UI backend)
A separate program (`ffa_server`) that puts the engine behind a small HTTP API on
`127.0.0.1` so the browser UI can drive it. Key ideas:
- **One background thread owns the camera and the Recognizer** (the models aren't
  thread-safe). It continuously reads frames, runs enroll/watch logic, and keeps
  the latest annotated JPEG ready.
- **HTTP handlers** (list people, start/stop watch, capture, delete…) talk to
  that thread through shared variables protected by a **mutex**. All database
  access goes through one `dbMutex` so it's safe.
- **`/stream.mjpg`** streams the annotated preview as MJPEG to an `<img>` tag.
- It serves the built React app from `ui/dist`.
- It does **not** speak — it exposes an "announcement event," and the browser
  does the talking (Web Speech). Bound to localhost only.

### The `ui/` folder — the React + TypeScript front-end
- `src/api.ts` — one typed function per server endpoint (the only place that
  knows the API shape).
- `src/App.tsx` — polls `/api/status` twice a second, holds the people list, and
  triggers browser speech when a new announcement arrives.
- `src/components/Preview.tsx` — shows the MJPEG stream.
- `src/components/WatchPanel.tsx` — Start/Stop watching + the result card
  ("Possible match" / "Not sure" / "Unknown").
- `src/components/EnrollPanel.tsx` — enter a name, then capture shots.
- `src/components/PeoplePanel.tsx` — list + delete enrolled people.
- `vite.config.ts` — in dev, proxies `/api` and `/stream.mjpg` to the C++ server.

### `engine/tests/test_safety.cpp` — the proof
15 GoogleTest unit tests for `cosineSim`, `classify`, `FrameVoter`, and
`passesQualityGate`. They run without the camera or models.

### `engine/CMakeLists.txt` — the build recipe
Builds a shared library `ffacore` (all the modules), then three programs:
`ffa` (the app), `ffa_eval` (tuning), `ffa_tests` (tests).

### `scripts/download_models.sh` — model fetcher
Downloads the YuNet and SFace `.onnx` files from the OpenCV Model Zoo into
`engine/models/`.

---

## 6. The recognition pipeline in detail

Walking through what happens for one face, with the file where it lives:

1. **Capture** — `Camera::read()` grabs a frame (only while `watch` is running).
   *(camera.cpp)*
2. **Detect** — `Recognizer::detect()` runs YuNet → boxes around faces.
   *(recognizer.cpp)*
3. **Pick one** — `largestFace()` chooses the biggest face (the person you're
   facing). We don't silently pick among many. *(main.cpp)*
4. **Quality gate** — `passesQualityGate()` rejects it if: detector unsure
   (`detScore` < 0.90), too small (< 80px), off-screen, or blurry (Laplacian
   variance < 40). *(safety.cpp)*
5. **Embed** — `Recognizer::embed()` aligns the face and runs SFace → 128
   numbers. *(recognizer.cpp)*
6. **Compare** — `classify()` computes cosine similarity to every enrolled
   embedding, keeps the best score per person. *(safety.cpp)*
7. **Decide** — apply threshold (0.54) and margin (0.10):
   - best < 0.54 → **Unknown**
   - best ≥ 0.54 but beats runner-up by < 0.10 → **Unsure**
   - else → **Match** (this person)
8. **Vote** — `FrameVoter` requires 5 consecutive Match frames for the same
   person. *(safety.cpp)*
9. **Announce** — `Announcer::announce()` speaks it once, then 2-minute cooldown.
   *(announcer.cpp)*

---

## 7. The safety logic in detail

Four independent guards, each catching a different failure. All are tunable in
`config.hpp` and tested in `test_safety.cpp`.

1. **Quality gate** — *don't match garbage.* A tiny/blurry/half-hidden face
   gives an unreliable embedding, so we reject it before comparing.
2. **Threshold (0.54)** — *don't match weak similarity.* The face must be
   genuinely close to a saved one, not just "the closest of a bad bunch."
3. **Margin (0.10)** — *don't guess between look-alikes.* If two enrolled people
   both score high and close, we can't tell them apart → say "unsure," not a
   coin-flip.
4. **Multi-frame agreement (5)** — *don't trust one lucky frame.* A single frame
   can be a fluke; requiring 5-in-a-row makes announcements stable.

Plus the **announcer cooldown (120s)** so it never repeats a name over and over.

**Why so cautious?** Because the cost of errors is asymmetric: staying silent is
mildly annoying; saying the *wrong* name is socially harmful. Every guard is
tuned toward silence-over-wrong.

---

## 8. How data is stored, and how to see it

Data lives in one SQLite file: **`data/faces.db`**. Two tables:

**`people`**
| column | meaning |
|---|---|
| `id` | unique number for the person |
| `name` | e.g. "Sarah" |
| `reminder` | e.g. "cousin" |
| `created_at` | when added |

**`embeddings`**
| column | meaning |
|---|---|
| `id` | unique row id |
| `person_id` | which person this belongs to |
| `vec` | the 128 numbers, stored as a BLOB (512 bytes = 128 × 4-byte floats) |
| `created_at` | when added |

### See the data — three ways

**A) Quickest — the app itself:**
```bash
cd engine/build
./ffa list          # shows each person + how many embeddings they have
```

**B) The SQLite command line** (built into macOS):
```bash
cd engine/build
sqlite3 ../../data/faces.db
```
Then at the `sqlite>` prompt:
```sql
.tables                                  -- list tables: people, embeddings
SELECT * FROM people;                    -- see all enrolled people
SELECT person_id, length(vec) FROM embeddings;   -- 512 bytes each = 128 floats
.quit
```
> The `vec` column is raw binary (the 128 numbers), so it won't print as readable
> text — `length(vec)` confirming 512 bytes is the useful check. `.mode column`
> and `.headers on` make output prettier.

**C) A visual database browser (optional GUI):**
Install *DB Browser for SQLite* (`brew install --cask db-browser-for-sqlite`),
open `data/faces.db`, and click through the tables in a spreadsheet-like view.

> **Privacy note:** `faces.db` is real biometric data. It's git-ignored and
> stays on your machine. `./ffa delete --all` wipes it.

---

## 9. How the AI "learns" a person

**We never train the neural networks.** YuNet and SFace are **pretrained** — we
only run them (inference). Training a face model from scratch needs millions of
images and serious hardware, and it's unnecessary here.

**"Teaching it Sarah" = enrollment, which is remembering, not training:**
1. Provide a few photos of Sarah.
2. Each photo → SFace → a 128-number embedding.
3. The embeddings are saved under "Sarah" in the database.
4. Later, a live face's embedding is compared to Sarah's saved ones.

The reason this works on people the model has never seen is **metric learning /
transfer learning**: SFace was trained so that *any* two photos of the same
person land close together in the 128-D space, and different people land far
apart — even for faces outside its training data.

**To improve recognition of a person, give more/better photos** (varied angle,
lighting, expression):
```bash
./ffa enroll-dir "Sarah" /path/to/sarah_photos --reminder "cousin"
```
That is the real lever — no ML training involved.

**Could you actually fine-tune the model?** Yes, but it's not recommended here:
lots of data + compute, easy to overfit a small friend group, little benefit over
enrollment. (The project brief also says not to train from scratch.)

---

## 10. Build, run, test, evaluate

**One-time setup:**
```bash
# install libraries (already done on this Mac)
brew install cmake pkg-config opencv sqlite googletest nlohmann-json cpp-httplib

# download the AI models
./scripts/download_models.sh
```

**Build:**
```bash
cd engine
cmake -S . -B build          # configure (once, or after editing CMakeLists)
cmake --build build          # compile → build/ffa, ffa_eval, ffa_tests, ffa_server
```

**Run the web app (easiest — builds everything and starts the server):**
```bash
./scripts/run.sh             # then open http://127.0.0.1:8765
```
This builds the C++ server, builds the React UI (first time), and starts
`ffa_server`. Open the URL in a browser to enroll, watch, and manage people.

**Front-end development (hot reload):**
```bash
cd engine/build && ./ffa_server        # terminal 1: the API + camera
cd ui && npm run dev                    # terminal 2: Vite dev server (proxies /api)
```

**Run the app** (from `engine/build`):
```bash
./ffa enroll "Sarah" --reminder "cousin"    # webcam capture (press SPACE ×5)
./ffa enroll-dir "Sarah" ./photos           # or enroll from an image folder
./ffa watch                                 # live hands-free recognition
./ffa identify ./photo.jpg                  # classify a single image (offline)
./ffa list                                  # who's enrolled
./ffa delete "Sarah"                        # remove one person
./ffa delete --all                          # wipe everything
```

**Run the tests:**
```bash
cd engine/build
ctest --output-on-failure      # or: ./ffa_tests
```

**Measure accuracy / re-tune** (needs a dataset: one folder per person):
```bash
cd engine/build
./ffa_eval /path/to/dataset --report ../../docs/EVALUATION.md
```

---

## 11. The evaluation results

Measured with `ffa_eval` on **LFW** (a public face benchmark), 25 enrolled
identities + 148 strangers:

| metric | result |
|---|---|
| Correct matches | 99.5% (186/187) |
| **Wrong-name matches** | **0** |
| Missed (said unknown for an enrolled person) | 1 |
| **False matches (stranger called a name)** | **0 / 1642** |
| Speed | ~8 ms per face (CPU) |

The tradeoff that justifies our threshold: OpenCV's default (0.363) would falsely
name **42** strangers; our tuned 0.54 drops that to **0**, costing one miss. Full
method + honest limitations in [EVALUATION.md](EVALUATION.md).

---

## 12. Interview prep

### The 30-second pitch
> "I built a local, privacy-first face-recognition aid for people with face
> blindness. It uses two pretrained neural networks — one to detect faces, one to
> turn a face into a 128-dimensional embedding — and then a transparent decision
> layer (cosine similarity plus a threshold, a margin between the top two
> candidates, and multi-frame agreement) that's tuned to stay silent rather than
> say a wrong name. I measured it on a public benchmark: zero false matches over
> 1600+ stranger tests. It's C++/OpenCV, all on-device, with unit tests and an
> evaluation harness."

### Questions you should be ready for
- **"How does the recognition work?"** → walk the assembly line (§3).
- **"Is it AI? Did you train it?"** → detection + embedding are pretrained CNNs
  (inference, not training); the decision is plain math/rules; enrollment stores
  embeddings, it doesn't retrain the model (§9).
- **"What's an embedding?"** → a 128-number fingerprint of a face; same person →
  close, different → far; compared with cosine similarity (§2).
- **"How do you avoid wrong names?"** → the four guards in §7, and you tuned the
  threshold with measured data (§11).
- **"How did you test it?"** → 15 unit tests on the decision logic + an
  evaluation harness on a held-out dataset with real metrics (§10, §11).
- **"What are the limitations?"** → LFW is cleaner than a real webcam; operating
  point chosen on the same set it's reported on; small enrolled set (be honest —
  it's in EVALUATION.md).
- **"Why C++?"** → single-language native app, OpenCV is natively C++, real-time,
  systems-level signal; the recognition logic is identical in any language.

### Things that make you sound like you built it
- You can name where each thing lives (§5).
- You can explain *why* it stays silent (asymmetric cost of errors).
- You quote real numbers and their limits, not marketing accuracy.

---

## 13. Roadmap

- ✅ **M1** Plan + prototype
- ✅ **M2** Recognition engine (offline-verified; live camera is a manual check)
- ✅ **M3** Safety behavior — tested + tuned
- ✅ **M5** Evaluation — pulled forward, real metrics exist
- 🟡 **M4** Usable UI — **built**: local API (cpp-httplib, 127.0.0.1) + React +
  TypeScript with live preview, enrollment, watch + result card, mute, camera
  indicator, manage/delete. Remaining: run it live on the Mac (camera permission)
  and polish.
- ⬜ **M6** Polish — onboarding, accessibility, docs, recorded demo
- ⬜ **Optional** Wearable (phone-as-camera over local Wi-Fi)

*(This handbook will be updated as each milestone lands.)*
