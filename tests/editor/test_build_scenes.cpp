#include <gtest/gtest.h>

#include "nexus/editor/build_scenes.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace nexus::editor::tests {

namespace fs = std::filesystem;

namespace {

fs::path tmp_path(const char* name) {
    return fs::temp_directory_path() / name;
}

}  // namespace

// ── Mutation: add / remove / contains ───────────────────────────────────────

TEST(BuildScenes, AddAppendsAndDedupes) {
    BuildScenes b;
    EXPECT_TRUE(b.add("Assets/Main.nxs"));
    EXPECT_TRUE(b.add("Assets/Level1.nxs"));
    EXPECT_FALSE(b.add("Assets/Main.nxs"));   // duplicate refused
    EXPECT_FALSE(b.add(""));                   // empty refused
    EXPECT_EQ(b.size(), 2u);
    EXPECT_TRUE(b.contains("Assets/Main.nxs"));
    EXPECT_FALSE(b.contains("Assets/Other.nxs"));
}

TEST(BuildScenes, RemoveReportsPresenceAndUpdatesSize) {
    BuildScenes b;
    b.add("a"); b.add("b"); b.add("c");
    EXPECT_TRUE(b.remove("b"));
    EXPECT_EQ(b.size(), 2u);
    EXPECT_FALSE(b.remove("nope"));
    EXPECT_EQ(b.scenes()[0], "a");
    EXPECT_EQ(b.scenes()[1], "c");
}

TEST(BuildScenes, RemovingStartupSnapsToTop) {
    BuildScenes b;
    b.add("a"); b.add("b"); b.add("c");
    b.set_startup_index(2);
    EXPECT_EQ(b.startup_scene(), "c");
    b.remove("c");
    EXPECT_EQ(b.startup_index(), 0u);  // snapped to top
    EXPECT_EQ(b.startup_scene(), "a");
}

TEST(BuildScenes, RemovingBeforeStartupShiftsIndex) {
    BuildScenes b;
    b.add("a"); b.add("b"); b.add("c");
    b.set_startup_index(2);
    b.remove("a");
    // "c" was at idx 2, "a" removed → "c" now at idx 1.
    EXPECT_EQ(b.startup_index(), 1u);
    EXPECT_EQ(b.startup_scene(), "c");
}

TEST(BuildScenes, RemovingAfterStartupLeavesIndexAlone) {
    BuildScenes b;
    b.add("a"); b.add("b"); b.add("c");
    b.set_startup_index(0);
    b.remove("c");
    EXPECT_EQ(b.startup_index(), 0u);
    EXPECT_EQ(b.startup_scene(), "a");
}

TEST(BuildScenes, ClearWipesEverything) {
    BuildScenes b;
    b.add("a"); b.add("b");
    b.set_startup_index(1);
    b.clear();
    EXPECT_TRUE(b.empty());
    EXPECT_EQ(b.startup_index(), 0u);
    EXPECT_EQ(b.startup_scene(), "");
}

// ── Reorder / move ──────────────────────────────────────────────────────────

TEST(BuildScenes, MoveReordersAndPreservesStartup) {
    BuildScenes b;
    b.add("a"); b.add("b"); b.add("c"); b.add("d");
    b.set_startup_index(2);  // points at "c"
    EXPECT_TRUE(b.move(0, 3));  // a → end
    // List is now [b, c, d, a].  Startup pointer should still point at "c".
    EXPECT_EQ(b.scenes()[1], "c");
    EXPECT_EQ(b.startup_scene(), "c");
}

TEST(BuildScenes, MoveOutOfRangeFails) {
    BuildScenes b;
    b.add("a"); b.add("b");
    EXPECT_FALSE(b.move(5, 0));
    EXPECT_FALSE(b.move(0, 5));
    EXPECT_TRUE(b.move(0, 0));  // identity is fine
}

TEST(BuildScenes, MoveStartupEntryUpdatesIndex) {
    BuildScenes b;
    b.add("a"); b.add("b"); b.add("c");
    b.set_startup_index(0);  // "a"
    EXPECT_TRUE(b.move(0, 2));
    EXPECT_EQ(b.scenes()[2], "a");
    EXPECT_EQ(b.startup_index(), 2u);
}

// ── Startup helpers ─────────────────────────────────────────────────────────

TEST(BuildScenes, SetStartupByPathMovesEntryToFront) {
    BuildScenes b;
    b.add("a"); b.add("b"); b.add("c");
    EXPECT_TRUE(b.set_startup("c"));
    EXPECT_EQ(b.scenes().front(), "c");
    EXPECT_EQ(b.startup_index(), 0u);
    EXPECT_EQ(b.startup_scene(), "c");
}

TEST(BuildScenes, SetStartupReturnsFalseForUnknown) {
    BuildScenes b;
    b.add("a");
    EXPECT_FALSE(b.set_startup("missing"));
    EXPECT_EQ(b.startup_scene(), "a");
}

TEST(BuildScenes, SetStartupIndexClampsAndHandlesEmpty) {
    BuildScenes b;
    b.set_startup_index(99);  // empty list — no-op
    EXPECT_EQ(b.startup_index(), 0u);

    b.add("a"); b.add("b");
    b.set_startup_index(99);  // clamps to last
    EXPECT_EQ(b.startup_index(), 1u);
}

TEST(BuildScenes, NextAfterWrapsAround) {
    BuildScenes b;
    b.add("a"); b.add("b"); b.add("c");
    EXPECT_EQ(b.next_after("a"), "b");
    EXPECT_EQ(b.next_after("c"), "a");  // wraps
    EXPECT_EQ(b.next_after("nope"), "");
}

TEST(BuildScenes, IndexOfReturnsSizeForUnknown) {
    BuildScenes b;
    b.add("a");
    EXPECT_EQ(b.index_of("a"), 0u);
    EXPECT_EQ(b.index_of("nope"), b.size());
}

// ── JSON round-trip ─────────────────────────────────────────────────────────

TEST(BuildScenes, JSONRoundTrip) {
    BuildScenes b;
    b.add("Assets/Main.nxs");
    b.add("Assets/Level1.nxs");
    b.add("Assets/Level2.nxs");
    b.set_startup_index(1);

    const std::string s = b.to_json_string();
    BuildScenes c;
    ASSERT_TRUE(c.from_json_string(s));
    EXPECT_EQ(c.size(), 3u);
    EXPECT_EQ(c.scenes()[0], "Assets/Main.nxs");
    EXPECT_EQ(c.scenes()[1], "Assets/Level1.nxs");
    EXPECT_EQ(c.startup_index(), 1u);
    EXPECT_EQ(c.startup_scene(), "Assets/Level1.nxs");
}

TEST(BuildScenes, FromJSONClampsBadStartupIndex) {
    BuildScenes b;
    EXPECT_TRUE(b.from_json_string(R"({"scenes":["a","b"], "startup_index": 99})"));
    EXPECT_EQ(b.startup_index(), 1u);
}

TEST(BuildScenes, FromJSONEmptyArrayResetsIndexToZero) {
    BuildScenes b;
    EXPECT_TRUE(b.from_json_string(R"({"scenes":[], "startup_index": 5})"));
    EXPECT_TRUE(b.empty());
    EXPECT_EQ(b.startup_index(), 0u);
}

TEST(BuildScenes, FromJSONInvalidIsAtomicNoMutation) {
    BuildScenes b;
    b.add("a"); b.add("b");
    EXPECT_FALSE(b.from_json_string("{ this is not json"));
    // Original state preserved.
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b.scenes()[0], "a");
}

TEST(BuildScenes, FromJSONEmptyStringFails) {
    BuildScenes b;
    EXPECT_FALSE(b.from_json_string(""));
}

// ── File round-trip ─────────────────────────────────────────────────────────

TEST(BuildScenes, SaveAndLoadFile) {
    const fs::path p = tmp_path("nexus_buildscenes_test.json");
    if (fs::exists(p)) fs::remove(p);

    BuildScenes b;
    b.add("a"); b.add("b");
    b.set_startup_index(1);
    ASSERT_TRUE(b.save_to_file(p.string()));
    ASSERT_TRUE(fs::exists(p));

    BuildScenes c;
    ASSERT_TRUE(c.load_from_file(p.string()));
    EXPECT_EQ(c.size(), 2u);
    EXPECT_EQ(c.startup_index(), 1u);

    fs::remove(p);
}

TEST(BuildScenes, LoadMissingFileReturnsFalseAndPreservesState) {
    BuildScenes b;
    b.add("a");
    EXPECT_FALSE(b.load_from_file("/nope/missing.json"));
    EXPECT_EQ(b.size(), 1u);  // unchanged
}

TEST(BuildScenes, SaveRefusesEmptyPath) {
    BuildScenes b;
    EXPECT_FALSE(b.save_to_file(""));
}

}  // namespace nexus::editor::tests
