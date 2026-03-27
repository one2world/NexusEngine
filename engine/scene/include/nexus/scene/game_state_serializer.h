#pragma once

#include <string>
#include <unordered_map>

namespace nexus {

// ---------------------------------------------------------------------------
// GameStateSerializer - save/load arbitrary key-value game state as JSON
// ---------------------------------------------------------------------------
class GameStateSerializer {
public:
    using StateMap = std::unordered_map<std::string, std::string>;

    /// Save a state map to a JSON file.
    static bool save(const std::string& filepath, const StateMap& state);

    /// Load a state map from a JSON file.  Returns empty map on failure.
    static StateMap load(const std::string& filepath);

    /// Save to a numbered slot (e.g. slot 0 -> "save_slot_0.json").
    /// base_dir is the directory where save files are stored.
    static bool save_slot(int slot, const StateMap& state,
                          const std::string& base_dir = "saves");

    /// Load from a numbered slot.
    static StateMap load_slot(int slot,
                              const std::string& base_dir = "saves");

    /// Build the file path for a given slot number and base directory.
    static std::string slot_filepath(int slot, const std::string& base_dir = "saves");
};

} // namespace nexus
