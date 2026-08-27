#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <cstring>

// Simple JSON parser for C++ (minimal implementation)
class ConfigLoader {
public:
    static std::map<std::string, std::string> load_config(const std::string& filepath) {
        std::map<std::string, std::string> config;
        std::ifstream file(filepath);
        
        if (!file.is_open()) {
            std::cerr << "[CONFIG] Failed to open: " << filepath << std::endl;
            return config;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Remove whitespace
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            if (line.empty() || line[0] == '{' || line[0] == '}' || line[0] == '[' || line[0] == ']') {
                continue;
            }
            
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Trim quotes and whitespace
                key.erase(0, key.find_first_not_of(" \t\n\r\""));
                key.erase(key.find_last_not_of(" \t\n\r\"") + 1);
                value.erase(0, value.find_first_not_of(" \t\n\r\""));
                value.erase(value.find_last_not_of(" \t\n\r\"") + 1);
                
                if (!key.empty() && key != "{") {
                    config[key] = value;
                }
            }
        }
        
        file.close();
        return config;
    }
    
    template<typename T>
    static T get_value(const std::map<std::string, std::string>& config, 
                       const std::string& key, T default_value) {
        auto it = config.find(key);
        if (it != config.end()) {
            std::string value = it->second;
            // Convert string to T (simplified)
            if constexpr (std::is_same_v<T, std::string>) {
                return value;
            } else if constexpr (std::is_same_v<T, int>) {
                return std::stoi(value);
            } else if constexpr (std::is_same_v<T, float>) {
                return std::stof(value);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::stod(value);
            } else if constexpr (std::is_same_v<T, bool>) {
                return value == "true" || value == "1";
            }
        }
        return default_value;
    }
};

#endif // CONFIG_LOADER_H