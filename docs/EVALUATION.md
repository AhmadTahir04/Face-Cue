# Evaluation Report

This documents how the recognition thresholds were tuned and measured. It was
produced by the `ffa_eval` tool (`engine/src/eval.cpp`), which runs the **real**
pipeline (YuNet detection → SFace embedding → cosine match + safety rules) over
a labeled dataset.

## Dataset

**LFW (Labeled Faces in the Wild), funneled** — a standard public face-recognition
benchmark. Used here **only for engineering validation**; it is not committed to
this repo and no LFW data ships with the product.

- 25 identities treated as **enrolled** (3 template images each)
- 148 other identities held out as **strangers** (never enrolled — negative cases)
- Test queries: **187** enrolled-positive + **1642** stranger-negative
- Images per identity capped at 20 to keep the set balanced

**Split discipline:** template images and test images are disjoint (a person's
first 3 images enroll them; their remaining images are test queries), and
strangers are entirely disjoint identities. So every number below is on data the
matcher did not "see" as an enrolled template.

## Headline results (tuned operating point)

Recommended: **cosThreshold = 0.54, margin = 0.10**

| metric | count | rate |
|---|---|---|
| Correct matches (enrolled) | 186 / 187 | 99.5% |
| **Wrong-name matches** | 0 | 0% |
| Missed (unknown/unsure for an enrolled person) | 1 | 0.5% |
| **False matches (stranger announced as enrolled)** | 0 / 1642 | 0% |
| Correctly rejected strangers | 1642 / 1642 | 100% |

- **Per-face compute** (detect + align + embed): **~8 ms** on CPU (Apple Silicon).
  End-to-end announce latency ≈ this × `framesNeeded` (5) + camera frame time —
  on the order of a fraction of a second.

## Why this threshold — the safety tradeoff

At margin 0.10, sweeping the cosine threshold:

| cosThreshold | correct | wrong-name | miss | false-match (of 1642) |
|---|---|---|---|---|
| 0.363 (OpenCV default) | 100% | 0 | 0% | **42** |
| 0.44 | 100% | 0 | 0% | 9 |
| 0.50 | 100% | 0 | 0% | 1 |
| **0.54 (chosen)** | 99.5% | 0 | 0.5% | **0** |

The point of the product is to **avoid wrong/false names**. OpenCV's default
threshold recognizes everyone but falsely announces 42 strangers as enrolled
people. Raising the threshold to 0.54 eliminates every false match, costing only
a single missed recognition. For a face-blindness aid, that is the right trade:
silence is acceptable, a confidently wrong name is not.

## Limitations (read before trusting these numbers)

- **The operating point was chosen on the same set it is reported on**, so these
  figures are optimistic — they are not an unbiased estimate of accuracy on a new
  population. A cleaner protocol (tune on one split, report on a second untouched
  split) is future work.
- **LFW is not your living room.** Funneled LFW images are roughly frontal and
  reasonably lit. Real webcam captures at a gathering (angles, motion blur, poor
  light) will score **lower** cosine similarity, so the real-world miss rate will
  be higher and the threshold may need to come **down**. Re-tune on your own
  enrolled group with `ffa_eval`.
- **Small enrolled set** (25). With more enrolled people, look-alikes get more
  likely; the margin rule and multi-frame agreement matter more.
- These numbers are for **single images**. Live `watch` mode additionally
  requires agreement over several frames, which should further reduce false
  announcements versus these per-image figures.
- A cosine score is **not** a calibrated probability and is never shown as a "%
  confidence" in the app.

## Reproduce

```bash
cd engine/build
./ffa_eval /path/to/dataset --enrolled 25 --max-strangers 150 \
    --max-per-id 20 --report ../../docs/EVALUATION.md
```

Dataset layout: one subfolder per person, images inside.
