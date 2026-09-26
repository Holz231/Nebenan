#!/usr/bin/env bash
# Build Nebenan (library, tests, benchmark, demo) in Release mode
set -e
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
echo "Fertig: build/bin/nebenan_demo"
