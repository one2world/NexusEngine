#pragma once

#include "nexus/core/types.h"

#include <string>
#include <vector>

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// BuildScenes — ordered list of scene paths shipped with the game
// ─────────────────────────────────────────────────────────────────────────────
//
// Mirrors Unity's "Scenes In Build" window: an editable ordered list of
// scene file paths.  Index 0 is the startup scene by convention.  The
// editor uses this for:
//
//   • Build Settings — what to package + what to load first.
//   • Scene navigation — "next scene" / "previous scene" shortcuts.
//   • Multi-scene workflows — additive load lists.
//
// Pure data + bookkeeping; no ImGui, no rendering.  Persists as a tiny
// JSON file so the project's build manifest survives across editor
// sessions.  Schema:
//   {
//     "startup_index": 0,
//     "scenes": ["Assets/Main.nxs", "Assets/Level1.nxs", ...]
//   }
//
// Failure-mode policy: the JSON loader is lenient — unknown keys are
// ignored, malformed startup indices clamp to range, and a corrupt file
// returns false (caller decides whether to overwrite or surface).
class BuildScenes {
public:
    BuildScenes() = default;

    // ── Mutation ────────────────────────────────────────────────────────
    /// Append a scene path.  No-op if already present (paths are unique).
    /// Returns true on insertion, false if duplicate.
    bool add(const std::string& path);

    /// Remove a scene path.  Adjusts startup_index if the removed entry
    /// was at or before the startup row.  Returns true if found.
    bool remove(const std::string& path);

    /// Reorder: move the entry at `from` to position `to`.  Both indices
    /// must be in range [0, size).  Returns false otherwise.
    bool move(u32 from, u32 to);

    /// Mark `path` as the startup scene (index 0 by convention — moves
    /// the entry to the top).  Returns false if `path` isn't in the list.
    bool set_startup(const std::string& path);

    /// Set the startup index by position; clamps to [0, size).  No-op on
    /// empty list.
    void set_startup_index(u32 idx);

    /// Drop everything.
    void clear() { scenes_.clear(); startup_index_ = 0; }

    // ── Queries ─────────────────────────────────────────────────────────
    const std::vector<std::string>& scenes() const { return scenes_; }
    u32 size() const { return static_cast<u32>(scenes_.size()); }
    bool empty() const { return scenes_.empty(); }
    bool contains(const std::string& path) const;
    u32 startup_index() const { return startup_index_; }
    /// "" when the list is empty.
    std::string startup_scene() const;
    /// Index of `path` in the list, or `size()` if not present.
    u32 index_of(const std::string& path) const;

    /// Walk to the next scene after `current` (modulo size).  Returns ""
    /// when the list is empty or `current` isn't tracked.
    std::string next_after(const std::string& current) const;

    // ── Persistence ─────────────────────────────────────────────────────
    /// Read JSON from disk.  Returns false on missing / corrupt file —
    /// the existing in-memory state is preserved on failure so a partial
    /// load can't silently truncate.
    bool load_from_file(const std::string& path);

    /// Write JSON to disk.  Returns false on I/O failure.
    bool save_to_file(const std::string& path) const;

    /// Serialise to / parse from a JSON string.  Used by tests and by
    /// the file methods above.
    std::string to_json_string() const;
    bool from_json_string(const std::string& json);

private:
    std::vector<std::string> scenes_;
    u32 startup_index_{0};
};

}  // namespace nexus::editor
