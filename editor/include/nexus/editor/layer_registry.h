#pragma once

#include "nexus/core/types.h"

#include <array>
#include <string>

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// LayerRegistry — names + visibility for the engine's 32 layers
// ─────────────────────────────────────────────────────────────────────────────
//
// `TagComponent::layer` is a 0..31 index that systems use to bucket entities
// (visibility, raycast, collision).  The engine treats layers as opaque bits;
// the editor needs to name them and toggle their visibility from the
// SceneView toolbar.  This registry owns:
//
//   • A 32-slot name table (configurable, defaults match Unity's standard
//     layers: Default / TransparentFX / IgnoreRaycast / Water / UI).
//   • A 32-bit `visible_mask_` — a packed bitset of layers that should
//     render this frame.  Bit i is layer i.
//
// Pure data + bit math — no ImGui, no rendering.  The viewport calls
// `is_visible(entity_layer)` while walking entities to skip hidden layers.
class LayerRegistry {
public:
    static constexpr u32 kLayerCount = 32u;

    LayerRegistry();

    // Layer naming.  Returns "" for empty / out-of-range slots.
    void set_name(u32 layer, std::string name);
    const std::string& name(u32 layer) const;
    bool is_named(u32 layer) const;

    // Visibility — bit `layer` in the mask.  Default state is "all visible".
    bool is_visible(u32 layer) const;
    void set_visible(u32 layer, bool visible);
    void show_all();
    void hide_all();

    u32 visible_mask() const { return visible_mask_; }
    void set_visible_mask(u32 mask) { visible_mask_ = mask; }

    // First user-defined layer index.  Layers 0..7 are reserved for engine
    // built-ins; 8..31 are free for the project to repurpose.  Matches
    // Unity's convention so docs / tutorials translate.
    static constexpr u32 kFirstUserLayer = 8u;

private:
    std::array<std::string, kLayerCount> names_;
    u32 visible_mask_{0xFFFFFFFFu};
};

// ─────────────────────────────────────────────────────────────────────────────
// LayoutPreset — pre-canned ImGui dock arrangements
// ─────────────────────────────────────────────────────────────────────────────
//
// Layouts in Unity's "Layout" dropdown set the dock-node split + which panels
// occupy each region.  We model the same idea: a preset is a small string
// id, with the actual ImGui layout serialized into imgui.ini under that id
// when the user saves.  The dropdown lets the user switch between presets
// without manually re-docking.
enum class LayoutPreset : u8 {
    Default,
    TwoByThree,
    FourSplit,
    Tall,
    Wide,
};

const char* layout_preset_name(LayoutPreset p);
const char* layout_preset_ini_filename(LayoutPreset p);

}  // namespace nexus::editor
