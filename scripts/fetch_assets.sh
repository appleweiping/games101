#!/usr/bin/env bash
# Fetch the optional model asset used by Assignment 3 (and available to A6):
# Keenan Crane's "spot" cow (the exact model the GAMES101 A3 uses).  It is a
# third-party asset, so it is git-ignored and downloaded here on demand.  If it is
# absent the assignments fall back to procedurally-generated geometry and still run.
set -euo pipefail
A="$(cd "$(dirname "$0")/.." && pwd)/assets"
mkdir -p "$A"
if [ ! -f "$A/spot/spot_triangulated.obj" ]; then
  echo "Fetching spot model (Keenan Crane) ..."
  curl -fL -o "$A/spot.zip" "https://www.cs.cmu.edu/~kmcrane/Projects/ModelRepository/spot.zip"
  unzip -o "$A/spot.zip" -d "$A" >/dev/null
  rm -f "$A/spot.zip"
fi
echo "Assets ready in $A/spot (spot_triangulated.obj + spot_texture.png)"
