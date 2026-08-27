#!/usr/bin/env python3
"""
Performance Benchmarking Script for Security Robot V2
======================================================
Measures FPS, CPU usage, memory, and latency
Usage: python3 benchmark.py
"""

import subprocess
import time
import json
import os
import sys
import psutil
import threading
from pathlib import Path

class RobotBenchmark:
    def __init__(self, binary_path="./build/security_robot", duration=30):
        self.binary_path = binary_path
        self.duration = duration
        self.process = None
        self.metrics = {
            "fps": [],
            "cpu_percent": [],
            "memory_mb": [],
            "latency_ms": [],
            "timestamp": []
        }
        self.running = False
        
    def start_robot(self):
        """Start the robot process"""
        print(f"🚀 Starting robot: {self.binary_path}")
        self.process = subprocess.Popen(
            [self.binary_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        print(f"✅ Robot started (PID: {self.process.pid})")
        self.running = True
        
    def monitor_performance(self):
        """Monitor CPU, memory, and FPS in background"""
        try:
            proc = psutil.Process(self.process.pid)
            start_time = time.time()
            
            while self.running and (time.time() - start_time) < self.duration:
                try:
                    # Get CPU and memory metrics
                    cpu_percent = proc.cpu_percent(interval=0.1)
                    memory_info = proc.memory_info()
                    memory_mb = memory_info.rss / 1024 / 1024
                    
                    # Try to read FPS from status file
                    fps = self._read_fps_from_status()
                    
                    # Record metrics
                    self.metrics["cpu_percent"].append(cpu_percent)
                    self.metrics["memory_mb"].append(memory_mb)
                    if fps:
                        self.metrics["fps"].append(fps)
                    self.metrics["timestamp"].append(time.time() - start_time)
                    
                    time.sleep(0.5)  # Sample every 500ms
                    
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    break
                    
        except Exception as e:
            print(f"⚠️ Error monitoring performance: {e}")
    
    def _read_fps_from_status(self):
        """Read FPS from robot status file"""
        status_file = "/tmp/robot_status.json"
        try:
            if os.path.exists(status_file):
                with open(status_file, 'r') as f:
                    data = json.load(f)
                    return data.get('fps', None)
        except:
            pass
        return None
    
    def run_benchmark(self):
        """Run the benchmark"""
        print(f"\n📊 Benchmarking for {self.duration} seconds...")
        print("=" * 60)
        
        # Start robot
        self.start_robot()
        
        # Start monitoring thread
        monitor_thread = threading.Thread(target=self.monitor_performance, daemon=True)
        monitor_thread.start()
        
        # Wait for benchmark duration
        try:
            time.sleep(self.duration)
        except KeyboardInterrupt:
            print("\n⏹️ Benchmark interrupted by user")
        
        self.running = False
        
        # Wait for monitor thread
        monitor_thread.join(timeout=2)
        
        # Stop robot
        print("\n🛑 Stopping robot...")
        self.process.terminate()
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
        
    def print_results(self):
        """Print benchmark results"""
        print("\n" + "=" * 60)
        print("📊 BENCHMARK RESULTS")
        print("=" * 60)
        
        if not self.metrics["cpu_percent"]:
            print("❌ No metrics collected. Robot may have crashed.")
            return
        
        # Calculate statistics
        avg_cpu = sum(self.metrics["cpu_percent"]) / len(self.metrics["cpu_percent"])
        max_cpu = max(self.metrics["cpu_percent"])
        min_cpu = min(self.metrics["cpu_percent"])
        
        avg_mem = sum(self.metrics["memory_mb"]) / len(self.metrics["memory_mb"])
        max_mem = max(self.metrics["memory_mb"])
        min_mem = min(self.metrics["memory_mb"])
        
        # Print results
        print(f"\n🔴 CPU USAGE:")
        print(f"   Average: {avg_cpu:.2f}%")
        print(f"   Max:     {max_cpu:.2f}%")
        print(f"   Min:     {min_cpu:.2f}%")
        
        print(f"\n💾 MEMORY USAGE:")
        print(f"   Average: {avg_mem:.2f} MB")
        print(f"   Max:     {max_mem:.2f} MB")
        print(f"   Min:     {min_mem:.2f} MB")
        
        if self.metrics["fps"]:
            avg_fps = sum(self.metrics["fps"]) / len(self.metrics["fps"])
            max_fps = max(self.metrics["fps"])
            min_fps = min(self.metrics["fps"])
            print(f"\n⚡ FPS (From Status File):")
            print(f"   Average: {avg_fps:.2f} FPS")
            print(f"   Max:     {max_fps:.2f} FPS")
            print(f"   Min:     {min_fps:.2f} FPS")
        
        # Performance grade
        print(f"\n🎯 PERFORMANCE GRADE:")
        if avg_cpu < 15:
            grade = "🟢 EXCELLENT"
        elif avg_cpu < 25:
            grade = "🟡 GOOD"
        elif avg_cpu < 40:
            grade = "🟠 FAIR"
        else:
            grade = "🔴 POOR"
        print(f"   {grade}")
        
        print("\n" + "=" * 60)
    
    def save_results(self, output_file="benchmark_results.json"):
        """Save results to JSON file"""
        with open(output_file, 'w') as f:
            json.dump({
                "duration_seconds": self.duration,
                "binary": self.binary_path,
                "metrics": self.metrics
            }, f, indent=2)
        print(f"\n💾 Results saved to {output_file}")

def main():
    # Check if binary exists
    binary = "./build/security_robot"
    if not os.path.exists(binary):
        print(f"❌ Binary not found: {binary}")
        print("   Run: ./build_optimized.sh")
        return
    
    # Run benchmark
    benchmark = RobotBenchmark(binary_path=binary, duration=30)
    benchmark.run_benchmark()
    benchmark.print_results()
    benchmark.save_results()

if __name__ == "__main__":
    main()
