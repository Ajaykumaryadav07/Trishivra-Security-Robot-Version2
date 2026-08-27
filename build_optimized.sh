#!/bin/bash
# ============================================================================
# OPTIMIZED BUILD SCRIPT FOR SECURITY ROBOT V2
# ============================================================================
# This script compiles the robot with all optimization flags enabled
# Usage: ./build_optimized.sh
# ============================================================================

set -e  # Exit on error

echo "🚀 Building Security Robot V2 (OPTIMIZED)..."
echo "=============================================="

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir -p build
    echo "✅ Created build directory"
fi

cd build

# ============================================================================
# CONFIGURE CMAKE WITH OPTIMIZATION FLAGS
# ============================================================================
echo ""
echo "📋 Configuring CMake with optimization flags..."

cmake -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_CXX_FLAGS_RELEASE="-O3 -march=native -DNDEBUG -flto -ffast-math -fno-strict-aliasing" \
       -DCMAKE_C_FLAGS_RELEASE="-O3 -march=native -DNDEBUG -flto -ffast-math" \
       -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
       ..

# ============================================================================
# BUILD WITH MULTIPLE CORES
# ============================================================================
echo ""
echo "🔨 Compiling (using $(nproc) cores)..."

# Get number of CPU cores
CORES=$(nproc)
make -j $CORES

# ============================================================================
# POST-COMPILE OPTIMIZATION
# ============================================================================
echo ""
echo "📦 Post-processing binary..."

# Strip debug symbols (50% size reduction)
if [ -f "security_robot" ]; then
    BEFORE=$(stat --format=%s security_robot)
    strip security_robot
    AFTER=$(stat --format=%s security_robot)
    REDUCTION=$((BEFORE - AFTER))
    PERCENT=$((REDUCTION * 100 / BEFORE))
    echo "✅ Binary stripped: $BEFORE → $AFTER bytes (saved ${PERCENT}%)"
fi

# ============================================================================
# VERIFY OPTIMIZATION
# ============================================================================
echo ""
echo "✅ Build complete!"
echo ""
echo "📊 Binary info:"
file ./security_robot
ls -lh ./security_robot

echo ""
echo "🎯 To run the optimized binary:"
echo "   cd .."
echo "   ./build/security_robot"
echo ""
echo "💡 Tips:"
echo "   • Use 'cmake -DCMAKE_BUILD_TYPE=Debug' for debugging with symbols"
echo "   • Monitor performance: top -p \$(pgrep security_robot)"
echo "   • Check FPS: grep 'FPS:' /tmp/robot_status.json"
echo ""
