#!/usr/bin/env bash
# Run every built assignment; each writes its PNG(s) + measured numbers into results/.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p results
for id in a0 a1 a2 a3 a4 a5 a6 a7 a8; do
  echo "==================== $id ===================="
  "./build/$id.exe"
done
echo "Done. See results/ for images and measured output."
