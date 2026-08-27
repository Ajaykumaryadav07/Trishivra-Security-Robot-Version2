#ifndef VOICE_CONTROLLER_H
#define VOICE_CONTROLLER_H

#include <string>
#include <functional>

// ============================================================
// Voice Controller API
// ============================================================

/**
 * Initialize the voice system
 * @param wake_word Wake word keyword (e.g., "porcupine")
 * @param stt_model Path to Vosk model directory
 * @param piper_model Path to Piper model file
 * @return true on success
 */
bool voice_init(const std::string& wake_word = "porcupine",
                const std::string& stt_model = "vosk-model-small-en-us-0.15",
                const std::string& piper_model = "en_US-lessac-medium.onnx");

/**
 * Start listening for voice commands (background thread)
 */
void voice_start_listening();

/**
 * Stop listening
 */
void voice_stop_listening();

/**
 * Get next voice command from queue
 * @return Command string, empty if none
 */
std::string voice_get_command();

/**
 * Check if there's a command in queue
 */
bool voice_has_command();

/**
 * Clear all pending commands
 */
void voice_clear_commands();

/**
 * Speak text using TTS
 * @param text Text to speak
 */
void voice_speak(const std::string& text);

/**
 * Set callback for command handling
 * @param callback Function called when a command is recognized
 */
void voice_set_callback(std::function<void(const std::string&)> callback);

/**
 * Check if voice is listening
 */
bool voice_is_listening();

/**
 * Set audio device (optional)
 */
void voice_set_audio_device(int device);

/**
 * Auto-start voice (called from main)
 */
void voice_auto_start();

#endif // VOICE_CONTROLLER_H