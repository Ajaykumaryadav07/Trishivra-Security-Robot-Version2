import time
from smbus2 import SMBus

# Try Bus 0 first. If it fails, change to SMBus(11)
try:
    bus = SMBus(0) 
    print("Connected to I2C Bus 0")
except Exception as e:
    print(f"Bus 0 failed, trying Bus 11...")
    bus = SMBus(11)

AS5600_ADDR = 0x36

def read_angle():
    try:
        high = bus.read_byte_data(AS5600_ADDR, 0x0C)
        low = bus.read_byte_data(AS5600_ADDR, 0x0D)
        raw_angle = (high << 8) | low
        degrees = (raw_angle * 360.0) / 4096.0
        return degrees
    except Exception as e:
        return None

print("Spin the Right Encoder magnet. Press Ctrl+C to stop.")
try:
    while True:
        angle = read_angle()
        if angle is not None:
            print(f"Right Encoder Angle: {angle:.2f}°")
        else:
            print("Error reading encoder. Check wiring!")
        time.sleep(0.2)
except KeyboardInterrupt:
    print("\nTest stopped.")