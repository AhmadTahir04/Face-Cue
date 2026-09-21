# Dependencies & Licenses

All dependencies are open-source and permissively licensed. This must be
reviewed again before any public distribution.

## Build / runtime libraries

| Dependency | Purpose | License |
|---|---|---|
| OpenCV (incl. `objdetect`, `dnn`) | camera, face detection, embeddings | Apache-2.0 |
| SQLite | local storage | Public Domain |
| nlohmann/json | JSON for the local API (later milestone) | MIT |
| cpp-httplib | localhost HTTP server (later milestone) | MIT |
| CMake | build system | BSD-3-Clause |
| React, TypeScript, Vite (UI, later) | user interface | MIT |

## Machine-learning models

Downloaded from the [OpenCV Model Zoo](https://github.com/opencv/opencv_zoo)
via `scripts/download_models.sh`. Not committed to git.

| Model | File | Role | License |
|---|---|---|---|
| YuNet | `face_detection_yunet_2023mar.onnx` | face **detection** | MIT |
| SFace | `face_recognition_sface_2021dec.onnx` | face **embeddings** | Apache-2.0 |

> Model licenses are recorded here from the OpenCV Model Zoo at time of writing.
> **Verify the current upstream license before distributing** the project.
