#!/bin/bash
echo "🔨 Building Security Robot V2..."
mkdir -p build
cd build
VCPKG_ROOT="$(cd .. && pwd)/../vcpkg"
cmake .. -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
make -j4
echo "✅ Build complete!"