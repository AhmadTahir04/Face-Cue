# Familiar Face Assistant

A **private, local-only** assistant that helps a user recognize people they
already know and who have **consented** to be enrolled — built as an aid for
people with prosopagnosia (face blindness).

> It is a recognition **aid**, not a medical device, and not a tool for
> identifying strangers. It only compares faces against people you enroll, and
> it is designed to stay silent rather than guess a wrong name.
> See [docs/PRIVACY.md](docs/PRIVACY.md).

> 📘 **New here? Read [docs/HANDBOOK.md](docs/HANDBOOK.md)** — a from-scratch,
> plain-English guide to the whole project: every technology, every file, how the
> AI works, how to inspect the data, and interview prep.

## Status: Milestones 1–4 built (engine tuned + tested; web UI done)

A tuned, unit-tested C++ recognition engine (0 false matches over 1642 stranger
tests — see [docs/EVALUATION.md](docs/EVALUATION.md)) with **two front-ends**: a
command-line tool and a **local web app** (C++ HTTP server on `127.0.0.1` +
React/TypeScript UI) for enrolling, hands-free watching, and managing people.

> One manual step remains for the maintainer: run it once on the Mac to grant
> camera access and confirm live recognition (automated tests + evaluation cover
> everything that runs without a physical camera).

## Run the web app (easiest)

```bash
./scripts/run.sh
```

Then open **http://127.0.0.1:8765** in a browser. Grant camera access when macOS
asks. Enroll a few people, then press **Start watching**.

## Architecture (short version)

C++ engine does everything locally: **Camera → Recognizer → Safety → Announcer**,
backed by **SQLite**. A future React UI will talk to it over a `127.0.0.1`-only
API. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

- **Detection:** YuNet (`FaceDetectorYN`)
- **Embeddings:** SFace (`FaceRecognizerSF`), 128-D
- **Matching:** cosine similarity + threshold + top-2 margin + multi-frame agreement
- **Storage:** SQLite (names, reminders, embeddings) — sensitive biometric data

## Requirements (macOS, Apple Silicon)

Installed via Homebrew:

```bash
brew install cmake pkg-config opencv nlohmann-json cpp-httplib sqlite
```

## Setup

```bash
# 1. Download the open-source face models (not committed; see docs/LICENSES.md)
./scripts/download_models.sh

# 2. Build the engine
cd engine
cmake -S . -B build
cmake --build build

# 3. Run from the build dir (model paths are relative to it)
cd build
```

> **Camera permission:** the first run opens the macOS camera-access prompt for
> your terminal. Allow it, or the webcam will not open.

## Usage

```bash
# Enrollment
./ffa enroll "Sarah" --reminder "cousin"     # webcam: press SPACE to capture 5 shots
./ffa enroll-dir "Sarah" ./photos/sarah      # offline: enroll from a folder of images

# Recognition
./ffa watch                                  # live, hands-free recognition
./ffa identify ./some_photo.jpg              # offline: classify one image

# Manage
./ffa list                                   # show enrolled people
./ffa delete "Sarah"                         # delete one person
./ffa delete --all                           # wipe all enrolled data
```

In `watch` mode it announces an enrolled person **once** (spoken via macOS
`say`, plus on-screen), then stays quiet during a cooldown; unknown faces get
**no** announcement.

## Tests

The safety-critical decision logic (cosine, threshold, margin, multi-frame
voter, quality gate) has unit tests that run **without** the ML model:

```bash
cd engine/build && ctest --output-on-failure    # or ./ffa_tests
```

## Evaluation & tuning

`ffa_eval` runs the real pipeline over a labeled image dataset (one subfolder
per person), sweeps the threshold/margin, and reports the metrics that matter —
correct matches, **wrong-name matches**, misses, **false matches for
strangers**, and per-face latency.

```bash
./ffa_eval /path/to/dataset --report ../../docs/EVALUATION.md
```

The current defaults in `config.hpp` were tuned this way; see
[docs/EVALUATION.md](docs/EVALUATION.md). Numbers on a public benchmark are a
sanity check — tuning on your own enrolled group is still recommended.

## Tuning & honesty

Thresholds live in [engine/include/ffa/config.hpp](engine/include/ffa/config.hpp)
and are **uncalibrated starting values**. They will be tuned and measured in the
Evaluation milestone. A similarity score is **not** a probability and is never
shown as a confidence percentage.

## Privacy

All data and processing stay on your machine. No cloud, no accounts, no
network at runtime. See [docs/PRIVACY.md](docs/PRIVACY.md).
