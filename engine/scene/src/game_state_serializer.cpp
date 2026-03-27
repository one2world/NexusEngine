#include "nexus/scene/game_state_serializer.h"
#include <fstream>
#include <sstream>

namespace nexus {

// Simple JSON-like serialization without requiring nlohmann/json dependency.
// Format: { "key1": "value1", "key2": "value2" }

static std::string escape_json_string(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

static std::string unescape_json_string(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '"':  out += '"';  ++i; break;
                case '\\': out += '\\'; ++i; break;
                case 'n':  out += '\n'; ++i; break;
                case 'r':  out += '\r'; ++i; break;
                case 't':  out += '\t'; ++i; break;
                default:   out += s[i];      break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

bool GameStateSerializer::save(const std::string& filepath, const StateMap& state) {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "{\n";
    size_t count = 0;
    for (auto& [key, value] : state) {
        file << "  \"" << escape_json_string(key) << "\": \""
             << escape_json_string(value) << "\"";
        if (++count < state.size()) file << ",";
        file << "\n";
    }
    file << "}\n";
    return file.good();
}

GameStateSerializer::StateMap GameStateSerializer::load(const std::string& filepath) {
    StateMap result;
    std::ifstream file(filepath);
    if (!file.is_open()) return result;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Simple parser: find "key": "value" pairs
    size_t pos = 0;
    while (pos < content.size()) {
        // Find opening quote for key
        size_t key_start = content.find('"', pos);
        if (key_start == std::string::npos) break;
        ++key_start;

        // Find closing quote for key (handle escapes)
        size_t key_end = key_start;
        while (key_end < content.size()) {
            if (content[key_end] == '\\') { key_end += 2; continue; }
            if (content[key_end] == '"') break;
            ++key_end;
        }
        if (key_end >= content.size()) break;

        std::string key = unescape_json_string(content.substr(key_start, key_end - key_start));

        // Find colon
        size_t colon = content.find(':', key_end + 1);
        if (colon == std::string::npos) break;

        // Find opening quote for value
        size_t val_start = content.find('"', colon + 1);
        if (val_start == std::string::npos) break;
        ++val_start;

        // Find closing quote for value
        size_t val_end = val_start;
        while (val_end < content.size()) {
            if (content[val_end] == '\\') { val_end += 2; continue; }
            if (content[val_end] == '"') break;
            ++val_end;
        }
        if (val_end >= content.size()) break;

        std::string value = unescape_json_string(content.substr(val_start, val_end - val_start));
        result[key] = value;
        pos = val_end + 1;
    }

    return result;
}

std::string GameStateSerializer::slot_filepath(int slot, const std::string& base_dir) {
    return base_dir + "/save_slot_" + std::to_string(slot) + ".json";
}

bool GameStateSerializer::save_slot(int slot, const StateMap& state,
                                     const std::string& base_dir) {
    return save(slot_filepath(slot, base_dir), state);
}

GameStateSerializer::StateMap GameStateSerializer::load_slot(int slot,
                                                              const std::string& base_dir) {
    return load(slot_filepath(slot, base_dir));
}

} // namespace nexus
