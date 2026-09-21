# Familiar Face Assistant

A **private, local-only** assistant that helps a user recognize people they
already know and who have **consented** to be enrolled — built as an aid for
people with prosopagnosia (face blindness).

> It is a recognition **aid**, not a medical device, and not a tool for
> identifying strangers. It only compares faces against people you enroll, and
> it is designed to stay silent rather than guess a wrong name.
> See [docs/PRIVACY.md](docs/PRIVACY.md).

## Status: Milestone 1 (command-line prototype)

A C++ command-line tool that enrolls a person from the webcam and, in a live
"watch" mode, announces an enrolled person or says nothing for an unknown face.
The polished React UI and localhost API come in later milestones.

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
./ffa enroll "Sarah" --reminder "cousin"   # capture 5 shots (press SPACE)
./ffa list                                 # show enrolled people
./ffa watch                                # live, hands-free recognition
./ffa delete "Sarah"                       # delete one person
./ffa delete --all                         # wipe all enrolled data
```

In `watch` mode it announces an enrolled person **once** (spoken via macOS
`say`, plus on-screen), then stays quiet during a cooldown; unknown faces get
**no** announcement.

## Tuning & honesty

Thresholds live in [engine/include/ffa/config.hpp](engine/include/ffa/config.hpp)
and are **uncalibrated starting values**. They will be tuned and measured in the
Evaluation milestone. A similarity score is **not** a probability and is never
shown as a confidence percentage.

## Privacy

All data and processing stay on your machine. No cloud, no accounts, no
network at runtime. See [docs/PRIVACY.md](docs/PRIVACY.md).
