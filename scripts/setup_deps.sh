#!/usr/bin/env bash
# Fetch the header-only third-party libraries this repo builds against:
#   * Eigen 3.4.0            — linear algebra (as in the official GAMES101 framework)
#   * stb_image / _write     — headless PNG load & save (replaces OpenCV highgui)
# They are third-party code, so they live under third_party/ and are git-ignored.
set -euo pipefail
TP="$(cd "$(dirname "$0")/.." && pwd)/third_party"
mkdir -p "$TP"

if [ ! -f "$TP/stb_image_write.h" ]; then
  echo "Fetching stb_image_write.h ..."
  curl -fL -o "$TP/stb_image_write.h" https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
fi
if [ ! -f "$TP/stb_image.h" ]; then
  echo "Fetching stb_image.h ..."
  curl -fL -o "$TP/stb_image.h" https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
fi
if [ ! -d "$TP/eigen" ]; then
  echo "Fetching Eigen 3.4.0 ..."
  curl -fL -o "$TP/eigen.tar.gz" https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
  tar -xzf "$TP/eigen.tar.gz" -C "$TP"
  mv "$TP/eigen-3.4.0" "$TP/eigen"
  rm -f "$TP/eigen.tar.gz"
fi
echo "Dependencies ready in $TP"
