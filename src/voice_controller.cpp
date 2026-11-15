#include "voice_controller.h"
#include <iostream>
#include <atomic>

namespace {
std::atomic<bool> listening{false};
}

bool voice_init(const std::string&, const std::string&, const std::string&) {
    return true;
}

void voice_start_listening() {
    listening = true;
}

void voice_stop_listening() {
    listening = false;
}

std::string voice_get_command() {
    return {};
}

bool voice_has_command() {
    return false;
}

void voice_clear_commands() {}

void voice_speak(const std::string& text) {
    std::cout << "[VOICE] " << text << std::endl;
}

void voice_set_callback(std::function<void(const std::string&)>) {}

bool voice_is_listening() {
    return listening;
}

void voice_set_audio_device(int) {}

void voice_auto_start() {
    voice_start_listening();
}