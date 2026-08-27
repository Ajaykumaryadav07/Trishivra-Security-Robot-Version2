import sys


def voice_speak(text):
    """Speak text using a simple Python TTS fallback."""
    try:
        import pyttsx3
    except ImportError:
        print(f"[TTS] {text}")
        return

    try:
        engine = pyttsx3.init()
        engine.say(text)
        engine.runAndWait()
    except Exception:
        print(f"[TTS] {text}")


def voice_listen():
    """Listen for speech using speech_recognition if available, otherwise fall back to text input."""
    try:
        import speech_recognition as sr
    except ImportError:
        print("⚠️ speech_recognition is not installed. Falling back to keyboard input.")
        response = input("Type the phrase you want to speak: ")
        return response.strip() if response.strip() else None

    recognizer = sr.Recognizer()
    try:
        with sr.Microphone() as source:
            print("🎤 Listening... Please speak clearly.")
            recognizer.adjust_for_ambient_noise(source, duration=0.5)
            audio = recognizer.listen(source, timeout=5)
    except Exception as exc:
        print(f"⚠️ Voice input unavailable: {exc}")
        response = input("Type the phrase you want to speak: ")
        return response.strip() if response.strip() else None

    try:
        return recognizer.recognize_google(audio)
    except sr.UnknownValueError:
        print("❌ Speech was not recognized.")
        return None
    except Exception as exc:
        print(f"⚠️ Speech recognition failed: {exc}")
        return None
