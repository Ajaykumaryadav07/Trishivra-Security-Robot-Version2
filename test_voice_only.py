# test_voice_only.py
import sys
import os
sys.path.insert(0, os.getcwd())

# Direct import (skip __init__.py)
from python.voice_controller import voice_speak, voice_listen

if __name__ == "__main__":
    print("🎤 Testing Voice System...")
    
    # Test TTS
    voice_speak("Hello bro! I am Trishul, your security robot.")
    
    # Test STT (optional)
    print("\n🎤 Say something (e.g., 'Hello')...")
    text = voice_listen()
    if text:
        print(f"🗣️ You said: {text}")
        voice_speak(f"You said: {text}")
    else:
        print("❌ No speech detected.")