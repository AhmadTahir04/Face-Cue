#!/usr/bin/env bash
# Build everything (if needed) and start the local web app.
# Then open http://127.0.0.1:8765 in a browser.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 1. Models
if [ ! -f "$ROOT/engine/models/face_recognition_sface_2021dec.onnx" ]; then
  "$ROOT/scripts/download_models.sh"
fi

# 2. Build the C++ server
cmake -S "$ROOT/engine" -B "$ROOT/engine/build" >/dev/null
cmake --build "$ROOT/engine/build" --target ffa_server

# 3. Build the React UI (first time only)
if [ ! -d "$ROOT/ui/dist" ]; then
  ( cd "$ROOT/ui" && npm install && npm run build )
fi

# 4. Run
echo "Open http://127.0.0.1:8765 in your browser (Ctrl-C to stop)."
cd "$ROOT/engine/build" && ./ffa_server
