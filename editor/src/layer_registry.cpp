#include "nexus/editor/layer_registry.h"

namespace nexus::editor {

namespace {
const std::string kEmpty;
}

LayerRegistry::LayerRegistry() {
    // Defaults mirror Unity's reserved layers so docs / tutorials transfer
    // unchanged.  Names live in slots 0..7; 8..31 stay empty for project
    // customization.
    names_[0] = "Default";
    names_[1] = "TransparentFX";
    names_[2] = "Ignore Raycast";
    names_[4] = "Water";
    names_[5] = "UI";
}

void LayerRegistry::set_name(u32 layer, std::string name) {
    if (layer >= kLayerCount) return;
    names_[layer] = std::move(name);
}

const std::string& LayerRegistry::name(u32 layer) const {
    if (layer >= kLayerCount) return kEmpty;
    return names_[layer];
}

bool LayerRegistry::is_named(u32 layer) const {
    if (layer >= kLayerCount) return false;
    return !names_[layer].empty();
}

bool LayerRegistry::is_visible(u32 layer) const {
    if (layer >= kLayerCount) return false;
    return (visible_mask_ & (1u << layer)) != 0u;
}

void LayerRegistry::set_visible(u32 layer, bool visible) {
    if (layer >= kLayerCount) return;
    if (visible) visible_mask_ |=  (1u << layer);
    else         visible_mask_ &= ~(1u << layer);
}

void LayerRegistry::show_all() { visible_mask_ = 0xFFFFFFFFu; }
void LayerRegistry::hide_all() { visible_mask_ = 0u; }

// ── Layout presets ──────────────────────────────────────────────────────────

const char* layout_preset_name(LayoutPreset p) {
    switch (p) {
        case LayoutPreset::Default:    return "Default";
        case LayoutPreset::TwoByThree: return "2 by 3";
        case LayoutPreset::FourSplit:  return "4 Split";
        case LayoutPreset::Tall:       return "Tall";
        case LayoutPreset::Wide:       return "Wide";
    }
    return "Default";
}

const char* layout_preset_ini_filename(LayoutPreset p) {
    // Each preset persists into its own ini file inside `imgui.ini`'s
    // working directory.  The host loads/saves via ImGui::LoadIniSettings
    // / ::SaveIniSettings — pure file-name bookkeeping lives here so the
    // panel layer stays free of disk I/O knowledge.
    switch (p) {
        case LayoutPreset::Default:    return "imgui.ini";
        case LayoutPreset::TwoByThree: return "imgui_2x3.ini";
        case LayoutPreset::FourSplit:  return "imgui_4split.ini";
        case LayoutPreset::Tall:       return "imgui_tall.ini";
        case LayoutPreset::Wide:       return "imgui_wide.ini";
    }
    return "imgui.ini";
}

}  // namespace nexus::editor
