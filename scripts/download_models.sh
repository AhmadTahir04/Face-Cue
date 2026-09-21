#!/usr/bin/env bash
# Download the open-source face models (YuNet detector + SFace recognizer)
# from the OpenCV Model Zoo into engine/models/.
#
# These are NOT committed to git (see .gitignore). Licenses: docs/LICENSES.md.
# The models are served via git-lfs, so we use the media.githubusercontent.com
# endpoint which returns the real binary (raw. would return an LFS pointer).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODEL_DIR="$SCRIPT_DIR/../engine/models"
mkdir -p "$MODEL_DIR"

BASE="https://media.githubusercontent.com/media/opencv/opencv_zoo/main/models"
DETECTOR="face_detection_yunet_2023mar.onnx"
RECOGNIZER="face_recognition_sface_2021dec.onnx"

fetch() {
  local url="$1" out="$2"
  echo "Downloading $(basename "$out") ..."
  curl -fSL "$url" -o "$out"
  # An LFS pointer is tiny (<1KB); a real model is much larger. Guard against it.
  local size
  size=$(stat -f%z "$out" 2>/dev/null || stat -c%s "$out")
  if [ "$size" -lt 10000 ]; then
    echo "ERROR: $out is only ${size} bytes — likely an LFS pointer, not the model." >&2
    exit 1
  fi
  echo "  ok (${size} bytes)"
}

fetch "$BASE/face_detection_yunet/$DETECTOR"      "$MODEL_DIR/$DETECTOR"
fetch "$BASE/face_recognition_sface/$RECOGNIZER"  "$MODEL_DIR/$RECOGNIZER"

echo "Models ready in $MODEL_DIR"
