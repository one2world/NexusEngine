#include <gtest/gtest.h>

#include "nexus/editor/layer_registry.h"

#include <cstring>
#include <string>

namespace nexus::editor::tests {

// ── Defaults match Unity reserved layers ────────────────────────────────────

TEST(LayerRegistry, BuiltinNamesPopulatedAndUserSlotsEmpty) {
    LayerRegistry reg;
    EXPECT_EQ(reg.name(0),  "Default");
    EXPECT_EQ(reg.name(1),  "TransparentFX");
    EXPECT_EQ(reg.name(2),  "Ignore Raycast");
    EXPECT_EQ(reg.name(4),  "Water");
    EXPECT_EQ(reg.name(5),  "UI");
    // Unused reserved slots should be empty (3, 6, 7) AND the user range
    // 8..31 must be empty out of the box so the project can name its own.
    EXPECT_TRUE(reg.name(3).empty());
    EXPECT_TRUE(reg.name(8).empty());
    EXPECT_TRUE(reg.name(31).empty());
}

TEST(LayerRegistry, IsNamedTracksWhetherSlotIsBlank) {
    LayerRegistry reg;
    EXPECT_TRUE(reg.is_named(0));    // "Default"
    EXPECT_FALSE(reg.is_named(8));   // empty user slot
    EXPECT_FALSE(reg.is_named(99));  // out of range
}

// ── Custom names ────────────────────────────────────────────────────────────

TEST(LayerRegistry, SetNameRoundTripsAndOutOfRangeIsNoop) {
    LayerRegistry reg;
    reg.set_name(8, "Enemies");
    EXPECT_EQ(reg.name(8), "Enemies");
    reg.set_name(99, "Bogus");  // out of range
    EXPECT_EQ(reg.name(99), "");
}

// ── Visibility mask ─────────────────────────────────────────────────────────

TEST(LayerRegistry, AllVisibleByDefault) {
    LayerRegistry reg;
    for (u32 i = 0; i < 32u; ++i) {
        EXPECT_TRUE(reg.is_visible(i)) << "layer " << i;
    }
    EXPECT_EQ(reg.visible_mask(), 0xFFFFFFFFu);
}

TEST(LayerRegistry, SetVisibleTogglesIndividualBits) {
    LayerRegistry reg;
    reg.set_visible(5, false);
    EXPECT_FALSE(reg.is_visible(5));
    EXPECT_TRUE(reg.is_visible(4));
    EXPECT_TRUE(reg.is_visible(6));
    reg.set_visible(5, true);
    EXPECT_TRUE(reg.is_visible(5));
}

TEST(LayerRegistry, ShowAllAndHideAllAreSymmetric) {
    LayerRegistry reg;
    reg.hide_all();
    EXPECT_EQ(reg.visible_mask(), 0u);
    for (u32 i = 0; i < 32u; ++i) {
        EXPECT_FALSE(reg.is_visible(i)) << "layer " << i;
    }
    reg.show_all();
    EXPECT_EQ(reg.visible_mask(), 0xFFFFFFFFu);
}

TEST(LayerRegistry, SetVisibleMaskRoundTrip) {
    LayerRegistry reg;
    reg.set_visible_mask(0b101u);
    EXPECT_TRUE(reg.is_visible(0));
    EXPECT_FALSE(reg.is_visible(1));
    EXPECT_TRUE(reg.is_visible(2));
    EXPECT_FALSE(reg.is_visible(3));
    EXPECT_EQ(reg.visible_mask(), 0b101u);
}

TEST(LayerRegistry, OutOfRangeVisibilityQueriesAreFalseAndNoCrash) {
    LayerRegistry reg;
    EXPECT_FALSE(reg.is_visible(32));
    EXPECT_FALSE(reg.is_visible(99));
    reg.set_visible(99, true);  // no-op
    EXPECT_EQ(reg.visible_mask(), 0xFFFFFFFFu);
}

// ── Layout presets ──────────────────────────────────────────────────────────

TEST(LayoutPreset, NamesAreUnique) {
    const char* d  = layout_preset_name(LayoutPreset::Default);
    const char* t2 = layout_preset_name(LayoutPreset::TwoByThree);
    const char* fs = layout_preset_name(LayoutPreset::FourSplit);
    const char* ta = layout_preset_name(LayoutPreset::Tall);
    const char* w  = layout_preset_name(LayoutPreset::Wide);
    EXPECT_STRNE(d,  t2);
    EXPECT_STRNE(t2, fs);
    EXPECT_STRNE(fs, ta);
    EXPECT_STRNE(ta, w);
    // Default is the spelled-out name.
    EXPECT_STREQ(d, "Default");
}

TEST(LayoutPreset, IniFilenamesAreUniquePerPreset) {
    const char* a = layout_preset_ini_filename(LayoutPreset::Default);
    const char* b = layout_preset_ini_filename(LayoutPreset::TwoByThree);
    const char* c = layout_preset_ini_filename(LayoutPreset::FourSplit);
    const char* d = layout_preset_ini_filename(LayoutPreset::Tall);
    const char* e = layout_preset_ini_filename(LayoutPreset::Wide);
    EXPECT_STRNE(a, b);
    EXPECT_STRNE(b, c);
    EXPECT_STRNE(c, d);
    EXPECT_STRNE(d, e);
    EXPECT_STREQ(a, "imgui.ini");  // default preset stays canonical
}

}  // namespace nexus::editor::tests
