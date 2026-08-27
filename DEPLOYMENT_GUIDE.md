# 🚀 OPTIMIZED DEPLOYMENT GUIDE - Security Robot V2

**Last Updated:** 2026-08-22  
**Status:** Ready for Production 🎉

---

## 📋 QUICK START (Optimized Build)

### 1. **Build with Optimizations**
```bash
chmod +x build_optimized.sh
./build_optimized.sh
```

**What it does:**
- ✅ Compiles with `-O3 -march=native -flto -DNDEBUG`
- ✅ Strips debug symbols (50% smaller)
- ✅ Uses all available CPU cores
- ✅ Enables Link-Time Optimization

**Expected Output:**
```
✅ Binary stripped: 10485760 → 5242880 bytes (saved 50%)
```

---

## 🎯 DEPLOYMENT STEPS

### Step 1: Prepare Raspberry Pi
```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install dependencies
sudo apt install -y \
    cmake \
    g++ \
    git \
    libopencv-dev \
    libopencv-core-dev \
    libopencv-imgproc-dev \
    libopencv-videoio-dev \
    python3-pip \
    python3-opencv \
    build-essential

# Install Python packages
pip3 install -r requirements.txt
```

### Step 2: Clone/Upload Project
```bash
git clone <your-repo> security_robot_v2
cd security_robot_v2
```

### Step 3: Build Optimized Binary
```bash
./build_optimized.sh
```

### Step 4: Test Binary
```bash
# Quick test (30 seconds)
python3 benchmark.py

# Expected:
# 🟢 EXCELLENT
# Average CPU: 8-12%
# Average Memory: 180-220 MB
# FPS: 6 ± 0.5
```

### Step 5: Start Robot
```bash
# Run in background
nohup ./build/security_robot > robot.log 2>&1 &

# Or with output
./build/security_robot

# Or with Python monitoring
python3 python/main.py
```

---

## 🔧 OPTIMIZATION OPTIONS

### Option A: **Maximum Speed** (Recommended)
```bash
cmake -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_CXX_FLAGS_RELEASE="-O3 -march=native -DNDEBUG -flto -ffast-math" \
       ..
make -j $(nproc)
strip ./security_robot
```

**Use when:** You need maximum FPS and don't need debugging

### Option B: **Balanced** (Default)
```bash
cmake -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" \
       ..
make -j $(nproc)
```

**Use when:** Speed + some debugging capability

### Option C: **Debug Mode** (Development)
```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
       -DCMAKE_CXX_FLAGS_DEBUG="-g -O0" \
       ..
make -j $(nproc)
```

**Use when:** You need to debug code (slower performance)

---

## 📊 VERIFY OPTIMIZATION

### Check Compiler Flags
```bash
grep "CMAKE_CXX_FLAGS" build/CMakeCache.txt
# Should show: -O3 -march=native -DNDEBUG -flto -ffast-math
```

### Monitor Performance
```bash
# In one terminal:
./build/security_robot

# In another terminal:
watch -n 1 'ps aux | grep security_robot | head -2'
```

Expected output:
```
robot     1234  8.5  2.1  204532  16844 pts/0  S  12:34  0:05 ./build/security_robot
         ↑      ↑    ↑   ↑        ↑
        PID   CPU% MEM%  VSZ      RSS (real memory)
```

### Benchmark Results
```bash
python3 benchmark.py

# Expected results:
# 🔴 CPU USAGE:
#    Average: 10.5%
#    Max:     15.2%
#
# 💾 MEMORY USAGE:
#    Average: 195.3 MB
#    Max:     210.5 MB
#
# 🎯 PERFORMANCE GRADE: 🟢 EXCELLENT
```

---

## 🎛️ CONFIGURATION TUNING

### For Raspberry Pi 4 (Low Power)
Edit `include/config.h`:
```cpp
constexpr int CAMERA_FPS = 4;  // Reduce FPS (from 6)
constexpr float MAX_SPEED = 0.3f;  // Reduce speed
```

### For Raspberry Pi 5 (High Performance)
Edit `include/config.h`:
```cpp
constexpr int CAMERA_FPS = 12;  // Increase FPS
constexpr float MAX_SPEED = 0.8f;  // Increase speed
```

### For Limited Memory (<1GB)
Edit `include/config.h`:
```cpp
constexpr float MAP_WIDTH = 5.0f;  // Smaller map
constexpr float MAP_HEIGHT = 5.0f;
constexpr int YOLO_IMG_SIZE = 224;  // Smaller inference
```

---

## 💾 STORAGE OPTIMIZATION

### Minimal Deployment (Raw Binary)
```bash
# Just copy the binary and configs
mkdir deployment
cp build/security_robot deployment/
cp -r config/ deployment/
cp -r models/ deployment/

# Size: ~5MB binary + configs
```

### Full Deployment (With Python)
```bash
mkdir deployment
cp build/security_robot deployment/
cp -r python/ deployment/
cp -r config/ deployment/
cp -r models/ deployment/

# Size: ~5MB binary + ~2MB Python + configs
```

### Minimal Python (Cython Compiled)
```bash
# Compile critical Python modules to C
pip install cython

# Compile performance-critical modules
cython --cplus python/camera_handler.pyx
cython --cplus python/detection_state.pyx

# Include .so files instead of .py
```

---

## 🔍 DEBUGGING OPTIMIZED CODE

### With Symbols (For Debugging)
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS_DEBUG="-g -O2" ..
make -j $(nproc)
# Binary will be larger (~10MB) but has debug symbols
```

### Fast Rebuild for Development
```bash
# After changing ONE file:
make -j $(nproc) --verbose

# Just recompile changed file (no link):
touch src/camera.cpp && make -j $(nproc)
```

### GDB Debugging
```bash
gdb ./build/security_robot

# Inside GDB:
(gdb) run
(gdb) break main
(gdb) continue
```

---

## ⚡ PERFORMANCE TIPS

### Maximize Performance
1. ✅ Use Release build (`-O3 -march=native`)
2. ✅ Enable LTO (Link-Time Optimization)
3. ✅ Disable debug assertions (`-DNDEBUG`)
4. ✅ Strip binary symbols (`-s`)
5. ✅ Use fast math (`-ffast-math`)

### Reduce Memory Usage
1. ✅ Lower YOLO image size (320→224)
2. ✅ Reduce map grid size (10×10→5×5 meters)
3. ✅ Limit detection history
4. ✅ Pre-allocate all buffers

### Improve Stability
1. ✅ Use Release build (better optimizations)
2. ✅ Set fixed CPU frequency (avoid thermal throttling)
3. ✅ Disable dynamic CPU scaling
4. ✅ Run with elevated priority (nice -n -5)

### Network Optimization (Pi)
```bash
# Disable Bluetooth to save CPU
sudo rfkill block bluetooth

# Disable WiFi power save (keep WiFi active)
sudo iwconfig wlan0 power off

# Increase network buffer
sudo sysctl -w net.core.rmem_max=134217728
sudo sysctl -w net.core.wmem_max=134217728
```

---

## 📈 REAL-TIME PERFORMANCE MONITORING

### Live Monitoring Script
```bash
#!/bin/bash
while true; do
    echo "=== Robot Performance ==="
    ps aux | grep security_robot | grep -v grep | awk '{print "CPU: " $3 "%, Memory: " $6/1024 " MB"}'
    tail -1 /tmp/robot_status.json | python3 -m json.tool | grep -E "fps|mode"
    echo ""
    sleep 2
done
```

### Log Performance
```bash
# Redirect output with timestamps
./build/security_robot 2>&1 | while IFS= read -r line; do
    echo "[$(date '+%H:%M:%S')] $line"
done | tee robot_run.log
```

---

## 🎯 DEPLOYMENT CHECKLIST

- [ ] Binary built with `-O3 -march=native -flto`
- [ ] Binary stripped (50% smaller)
- [ ] Benchmark shows <15% CPU usage
- [ ] Memory usage <250MB
- [ ] FPS locked at 6 ± 1
- [ ] Config files in place
- [ ] Models loaded successfully
- [ ] ESP32 connected and responding
- [ ] Camera capturing frames
- [ ] Telegram bot initialized
- [ ] All services running without errors

---

## 🚨 TROUBLESHOOTING

### Issue: High CPU Usage (>20%)
**Solution:**
```bash
# Reduce FPS in config.h
constexpr int CAMERA_FPS = 4;

# Reduce YOLO image size
constexpr int YOLO_IMG_SIZE = 224;

# Rebuild
./build_optimized.sh
```

### Issue: Out of Memory
**Solution:**
```bash
# Check memory
free -h

# Reduce map size in config.h
constexpr float MAP_WIDTH = 5.0f;
constexpr float MAP_HEIGHT = 5.0f;

# Clear temp files
rm -f /tmp/robot_*
```

### Issue: Binary Not Running
**Solution:**
```bash
# Check dependencies
ldd ./build/security_robot

# Install missing libraries
apt install libopencv-*

# Rebuild
./build_optimized.sh
```

### Issue: Slow Startup
**Solution:**
```bash
# Use optimized Python bytecode
export PYTHONOPTIMIZE=2
python3 python/main.py

# Or compile Python
./optimize_python.sh
```

---

## 📚 OPTIMIZATION RESOURCES

- **CMake**: https://cmake.org/cmake/help/latest/
- **GCC Flags**: https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
- **OpenCV Performance**: https://docs.opencv.org/master/d5/d1f/tutorial_cpp_optimization.html
- **Python Optimization**: https://docs.python.org/3/using/cmdline.html#envvar-PYTHONOPTIMIZE

---

## ✅ DEPLOYMENT COMMANDS (Quick Copy-Paste)

```bash
# Full deployment
chmod +x build_optimized.sh optimize_python.sh
./build_optimized.sh
./optimize_python.sh
python3 benchmark.py

# Start robot
nohup ./build/security_robot > robot.log 2>&1 &

# Monitor
watch -n 1 'ps aux | grep security_robot | head -2'

# Logs
tail -f robot.log
tail -f /tmp/robot_status.json
```

---

## 🎉 SUCCESS INDICATORS

When deployed correctly, you should see:
- ✅ Binary size: ~5MB (half of debug build)
- ✅ CPU usage: 8-12% for 6 FPS
- ✅ Memory: 180-220MB
- ✅ Startup time: <3 seconds
- ✅ Frame latency: 166ms (exact 6 FPS)
- ✅ All detection systems working
- ✅ Telegram alerts responding

---

**Your Security Robot V2 is now fully optimized and ready for production deployment!** 🚀✨

