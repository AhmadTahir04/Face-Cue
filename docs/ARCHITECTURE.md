# Architecture

Familiar Face Assistant is a **local-only** app that helps a user recognize people
who have consented to be enrolled. It never contacts a network at runtime.

## High-level shape

```
        ┌───────────────────────────────────────────────┐
        │                 React + TypeScript UI           │
        │   preview · enroll · live-watch · manage/delete  │
        └───────────────▲───────────────────┬─────────────┘
                         │ HTTP / MJPEG      │  (all on 127.0.0.1 only)
                         │                   ▼
        ┌───────────────────────────────────────────────┐
        │              C++ engine (this repo, /engine)     │
        │                                                  │
        │  Camera ─► Recognizer ─► Safety ─► Announcer      │
        │                │                                 │
        │              Storage (SQLite, local files)       │
        └───────────────────────────────────────────────┘
```

The C++ engine is the brain. The React UI (added in a later milestone) is pure
presentation and talks to the engine over a localhost-only API. For Milestone 1
the engine runs as a **command-line prototype** — no server, no UI yet.

## Modules (deliberately separate)

| Module | File(s) | Responsibility |
|---|---|---|
| **Camera** | `camera.*` | Grab frames from a source. Today: laptop webcam. Later: phone/Pi stream — swap this module only. |
| **Recognizer** | `recognizer.*` | Detect faces (YuNet), align + embed (SFace), cosine-compare embeddings. |
| **Safety** | `safety.*` | The decision rules: quality gate, threshold, top-2 margin, match/unknown/unsure. |
| **Storage** | `storage.*` | SQLite: enrolled people, reminders, embeddings. Delete-one / delete-all. |
| **Announcer** | `announcer.*` | Speak/print a result at most once per person, then cooldown. Prevents repeat spam. |

Keeping these separate is what makes the future wearable possible **without
rewriting recognition** — only the Camera module changes.

## Recognition pipeline (per frame)

1. Capture a frame (only while watching / on trigger).
2. Detect faces with **YuNet** (`FaceDetectorYN`).
3. **Quality gate** — reject faces that are too small, low detector score, or too blurry.
4. Align + crop each usable face and compute a 128-D **SFace** embedding (`FaceRecognizerSF`).
5. Compare against enrolled embeddings via **cosine similarity**.
6. Apply a **threshold** and a **margin** over the second-best *person*.
7. Require **agreement over several consecutive frames** before announcing.
8. Otherwise return **unknown** / **unsure** (stay silent — never guess).

## Why "continuous but polite"

The app watches continuously (hands-free), but the **Announcer** enforces:
- announce each enrolled person **once**, then a cooldown before repeating;
- **silence** for unknown faces (no "unknown person" spam);
- conservative matching — a wrong name is worse than saying nothing.

## Thresholds are uncalibrated defaults

The numbers in `config.hpp` (threshold, margin, quality limits, frames-needed)
are **starting points, not validated values**. They will be tuned on one data
split and evaluated on a separate split during the Evaluation milestone. A
similarity score is **not** a calibrated probability and is never shown as a "%".
