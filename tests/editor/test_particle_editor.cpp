#include <gtest/gtest.h>

#include "nexus/editor/editor_tools.h"

#include <filesystem>
#include <fstream>

namespace nexus::editor::tests {

namespace fs = std::filesystem;

namespace {
fs::path tmp_path(const char* name) {
    return fs::temp_directory_path() / name;
}
}  // namespace

// ── sanitize() — clamp + swap inverted ranges ──────────────────────────────

TEST(ParticleEditorPanel, SanitizeClampsNegativesAndZeroes) {
    ParticlePreset p;
    p.spread        = -10.0f;
    p.min_speed     = -1.0f;
    p.max_speed     = -2.0f;
    p.min_lifetime  = -0.5f;
    p.max_lifetime  = -1.0f;
    p.start_size    = -0.1f;
    p.end_size      = -1.0f;
    p.emission_rate = -100.0f;
    ParticleEditorPanel::sanitize(p);
    EXPECT_FLOAT_EQ(p.spread,        0.0f);
    EXPECT_FLOAT_EQ(p.min_speed,     0.0f);
    EXPECT_FLOAT_EQ(p.max_speed,     0.0f);
    EXPECT_FLOAT_EQ(p.min_lifetime,  0.0f);
    EXPECT_FLOAT_EQ(p.max_lifetime,  0.0f);
    EXPECT_FLOAT_EQ(p.start_size,    0.0f);
    EXPECT_FLOAT_EQ(p.end_size,      0.0f);
    EXPECT_FLOAT_EQ(p.emission_rate, 0.0f);
}

TEST(ParticleEditorPanel, SanitizeSwapsInvertedRanges) {
    ParticlePreset p;
    p.min_speed    = 5.0f;
    p.max_speed    = 1.0f;
    p.min_lifetime = 3.0f;
    p.max_lifetime = 0.5f;
    ParticleEditorPanel::sanitize(p);
    EXPECT_FLOAT_EQ(p.min_speed,    1.0f);
    EXPECT_FLOAT_EQ(p.max_speed,    5.0f);
    EXPECT_FLOAT_EQ(p.min_lifetime, 0.5f);
    EXPECT_FLOAT_EQ(p.max_lifetime, 3.0f);
}

TEST(ParticleEditorPanel, SanitizeIsIdempotent) {
    ParticlePreset p;
    p.min_speed    = 1.0f;
    p.max_speed    = 5.0f;
    p.spread       = 30.0f;
    p.emission_rate = 100.0f;
    const ParticlePreset original = p;
    ParticleEditorPanel::sanitize(p);
    ParticleEditorPanel::sanitize(p);  // second run must be a no-op
    EXPECT_FLOAT_EQ(p.min_speed,     original.min_speed);
    EXPECT_FLOAT_EQ(p.max_speed,     original.max_speed);
    EXPECT_FLOAT_EQ(p.spread,        original.spread);
    EXPECT_FLOAT_EQ(p.emission_rate, original.emission_rate);
}

// ── Built-in catalogue ─────────────────────────────────────────────────────

TEST(ParticleEditorPanel, BuiltinPresetsContainCanonicalEntries) {
    const auto presets = ParticleEditorPanel::builtin_presets();
    ASSERT_EQ(presets.size(), 4u);

    auto find = [&](const std::string& name) {
        for (const auto& p : presets) if (p.name == name) return &p;
        return static_cast<const ParticlePreset*>(nullptr);
    };
    EXPECT_NE(find("Fire"),   nullptr);
    EXPECT_NE(find("Smoke"),  nullptr);
    EXPECT_NE(find("Sparks"), nullptr);
    EXPECT_NE(find("Magic"),  nullptr);
}

TEST(ParticleEditorPanel, BuiltinPresetsAreSelfConsistent) {
    // Each preset's min_* must be <= max_* and rates must be non-negative —
    // running sanitize on a built-in must be a no-op.
    auto presets = ParticleEditorPanel::builtin_presets();
    for (auto& p : presets) {
        ParticlePreset before = p;
        ParticleEditorPanel::sanitize(p);
        EXPECT_FLOAT_EQ(p.min_speed,    before.min_speed);
        EXPECT_FLOAT_EQ(p.max_speed,    before.max_speed);
        EXPECT_FLOAT_EQ(p.min_lifetime, before.min_lifetime);
        EXPECT_FLOAT_EQ(p.max_lifetime, before.max_lifetime);
    }
}

TEST(ParticleEditorPanel, SeedBuiltinPresetsPopulatesCatalogue) {
    ParticleEditorPanel pep;
    EXPECT_EQ(pep.preset_count(), 0u);
    pep.seed_builtin_presets();
    EXPECT_EQ(pep.preset_count(), 4u);
}

// ── load_preset() ──────────────────────────────────────────────────────────

TEST(ParticleEditorPanel, LoadPresetUpdatesCurrentAndSelection) {
    ParticleEditorPanel pep;
    pep.seed_builtin_presets();
    pep.load_preset(2);  // "Sparks"
    EXPECT_EQ(pep.current_preset().name, "Sparks");
    EXPECT_EQ(pep.selected_preset_index(), 2);
}

TEST(ParticleEditorPanel, LoadPresetOutOfRangeIsNoop) {
    ParticleEditorPanel pep;
    pep.seed_builtin_presets();
    pep.load_preset(2);
    pep.load_preset(99);  // out of range — no change
    EXPECT_EQ(pep.current_preset().name, "Sparks");
    EXPECT_EQ(pep.selected_preset_index(), 2);
}

// ── JSON round-trip ────────────────────────────────────────────────────────

TEST(ParticleEditorPanel, JSONRoundTripPreservesCatalogueAndCurrent) {
    ParticleEditorPanel pep;
    pep.seed_builtin_presets();
    auto cur = pep.builtin_presets().front();
    cur.name = "Custom Fire";
    cur.emission_rate = 999.0f;
    pep.set_current_preset(cur);

    const std::string s = pep.save_to_json();
    ASSERT_FALSE(s.empty());

    ParticleEditorPanel reload;
    ASSERT_TRUE(reload.load_from_json(s));
    EXPECT_EQ(reload.preset_count(), 4u);
    EXPECT_EQ(reload.current_preset().name, "Custom Fire");
    EXPECT_FLOAT_EQ(reload.current_preset().emission_rate, 999.0f);
}

TEST(ParticleEditorPanel, LoadCorruptJSONIsAtomicNoMutation) {
    ParticleEditorPanel pep;
    pep.seed_builtin_presets();
    EXPECT_FALSE(pep.load_from_json("{ this is not json"));
    // Existing presets must be untouched.
    EXPECT_EQ(pep.preset_count(), 4u);
}

TEST(ParticleEditorPanel, LoadEmptyJSONFails) {
    ParticleEditorPanel pep;
    EXPECT_FALSE(pep.load_from_json(""));
}

TEST(ParticleEditorPanel, JSONLoadSanitizesIncoming) {
    // Inverted speed range in the JSON — load should swap them so the
    // catalogue stays well-formed.
    const std::string raw = R"({
      "current": {"name":"X"},
      "presets": [{
        "name": "Inverted",
        "min_speed": 9.0,
        "max_speed": 1.0,
        "spread": -10.0,
        "emission_rate": -50.0
      }]
    })";
    ParticleEditorPanel pep;
    ASSERT_TRUE(pep.load_from_json(raw));
    ASSERT_EQ(pep.preset_count(), 1u);
    const auto& p = pep.presets().front();
    EXPECT_FLOAT_EQ(p.min_speed,     1.0f);
    EXPECT_FLOAT_EQ(p.max_speed,     9.0f);
    EXPECT_FLOAT_EQ(p.spread,        0.0f);
    EXPECT_FLOAT_EQ(p.emission_rate, 0.0f);
}

// ── File round-trip ────────────────────────────────────────────────────────

TEST(ParticleEditorPanel, SaveAndLoadFile) {
    const fs::path p = tmp_path("nexus_particle_test.json");
    if (fs::exists(p)) fs::remove(p);

    ParticleEditorPanel src;
    src.seed_builtin_presets();
    auto cur = ParticleEditorPanel::builtin_presets().back();
    cur.name = "Tweaked Magic";
    src.set_current_preset(cur);
    ASSERT_TRUE(src.save_to_file(p.string()));
    ASSERT_TRUE(fs::exists(p));

    ParticleEditorPanel dst;
    ASSERT_TRUE(dst.load_from_file(p.string()));
    EXPECT_EQ(dst.preset_count(), 4u);
    EXPECT_EQ(dst.current_preset().name, "Tweaked Magic");

    fs::remove(p);
}

TEST(ParticleEditorPanel, LoadMissingFileFails) {
    ParticleEditorPanel pep;
    pep.seed_builtin_presets();
    EXPECT_FALSE(pep.load_from_file("/nope/missing.json"));
    // Still has the built-ins (atomic load preserved state).
    EXPECT_EQ(pep.preset_count(), 4u);
}

TEST(ParticleEditorPanel, SaveRefusesEmptyPath) {
    ParticleEditorPanel pep;
    EXPECT_FALSE(pep.save_to_file(""));
}

// ── ClearPresets ───────────────────────────────────────────────────────────

TEST(ParticleEditorPanel, ClearPresetsResetsSelection) {
    ParticleEditorPanel pep;
    pep.seed_builtin_presets();
    pep.load_preset(1);
    EXPECT_EQ(pep.selected_preset_index(), 1);
    pep.clear_presets();
    EXPECT_EQ(pep.preset_count(), 0u);
    EXPECT_EQ(pep.selected_preset_index(), -1);
}

}  // namespace nexus::editor::tests
