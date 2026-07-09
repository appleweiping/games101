#!/usr/bin/env bash
# Compile every assignment to build/<id>.exe with g++ (C++17, -O2, OpenMP).
# Run scripts/setup_deps.sh first to fetch Eigen + stb.
set -euo pipefail
cd "$(dirname "$0")/.."
INC="-Ithird_party/eigen -Ithird_party -Icommon"
FLAGS="-std=c++17 -O2 -fopenmp"
mkdir -p build

build_one() { # <id> <dir>
  echo ">> building $1 ($2)"
  g++ $FLAGS $INC "assignments/$2/main.cpp" common/stb_impl.cpp -o "build/$1.exe"
}

build_one a0 a0_transform
build_one a1 a1_triangle
build_one a2 a2_rasterizer
build_one a3 a3_shading
build_one a4 a4_bezier
build_one a5 a5_whitted
build_one a6 a6_bvh
build_one a7 a7_pathtracing
echo "All assignments built into build/"
