#!/bin/bash
# ============================================================================
# PYTHON OPTIMIZATION SCRIPT FOR SECURITY ROBOT V2
# ============================================================================
# Compiles Python modules to bytecode for faster execution
# Usage: ./optimize_python.sh
# ============================================================================

set -e

echo "🐍 Optimizing Python modules..."
echo "===================================="

cd python

# ============================================================================
# COMPILE PYTHON MODULES TO BYTECODE
# ============================================================================
echo ""
echo "📦 Compiling Python to .pyc bytecode..."

python3 -m py_compile *.py
echo "✅ Python modules compiled to bytecode"

# ============================================================================
# OPTIMIZE WITH COMPRESSION
# ============================================================================
echo ""
echo "🗜️ Creating optimized .pyo files..."

# Compile with optimizations (-O)
python3 -O -m compileall -b .

echo "✅ Optimized bytecode created"

# ============================================================================
# CHECK OPTIMIZATION
# ============================================================================
echo ""
echo "📊 Bytecode files created:"
find . -name "*.pyc" -o -name "*.pyo" | head -10

# ============================================================================
# PERFORMANCE TIP
# ============================================================================
echo ""
echo "💡 To use optimized bytecode, set PYTHONOPTIMIZE:"
echo "   export PYTHONOPTIMIZE=2"
echo "   python3 main.py"
echo ""
echo "✅ Python optimization complete!"
echo ""
