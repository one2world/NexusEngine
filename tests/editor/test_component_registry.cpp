#include <gtest/gtest.h>

#include "nexus/editor/component_registry.h"
#include "nexus/scene/scene.h"
#include "nexus/scene/components.h"
#include "nexus/scene/registry.h"

namespace nexus::editor::tests {

// ── Built-in catalog ────────────────────────────────────────────────────────

TEST(ComponentRegistry, BuiltinsRegisterAcrossExpectedCategories) {
    ComponentRegistry reg;
    register_builtin_components(reg);

    EXPECT_GT(reg.size(), 12u);  // 17 today, leaving slack for future entries

    const auto cats = reg.categories();
    auto has_cat = [&](const std::string& name) {
        return std::find(cats.begin(), cats.end(), name) != cats.end();
    };
    EXPECT_TRUE(has_cat("Layout"));
    EXPECT_TRUE(has_cat("Mesh"));
    EXPECT_TRUE(has_cat("Rendering"));
    EXPECT_TRUE(has_cat("Physics"));
    EXPECT_TRUE(has_cat("Audio"));
}

// ── find() lookup ───────────────────────────────────────────────────────────

TEST(ComponentRegistry, FindByTypeIdReturnsExpectedDescriptor) {
    ComponentRegistry reg;
    register_builtin_components(reg);
    const auto* d = reg.find("Mesh Renderer");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->name, "Mesh Renderer");
    EXPECT_EQ(d->category, "Mesh");
    EXPECT_EQ(reg.find("DoesNotExist"), nullptr);
}

// ── Search filter (case-insensitive substring) ──────────────────────────────

TEST(ComponentRegistry, MatchingFiltersByNameOrCategoryCaseInsensitive) {
    ComponentRegistry reg;
    register_builtin_components(reg);

    const auto rb = reg.matching("rigid");
    EXPECT_GE(rb.size(), 2u);  // Rigid Body 2D + Rigid Body 3D

    const auto by_cat = reg.matching("Physics");
    EXPECT_GE(by_cat.size(), 4u);

    const auto empty = reg.matching("");  // empty query matches everything
    EXPECT_EQ(empty.size(), reg.size());

    const auto none = reg.matching("zzzzzz");
    EXPECT_TRUE(none.empty());
}

// ── available_to() — hides already-attached components ─────────────────────

TEST(ComponentRegistry, AvailableToHidesAlreadyAttachedComponents) {
    ComponentRegistry reg;
    register_builtin_components(reg);

    Scene scene;
    Entity e = scene.create_entity_3d("Cube");
    // create_entity_3d gives the entity a Transform3D + Tag.  Mesh Renderer
    // is NOT yet attached, so it should appear in the "available" list.
    auto& r = scene.registry();
    EXPECT_FALSE(r.has_component<MeshRendererComponent>(e));

    auto avail = reg.available_to(r, static_cast<u64>(e), "Mesh Renderer");
    bool saw_mr = false;
    for (auto* d : avail) {
        if (d->type_id == "Mesh Renderer") saw_mr = true;
    }
    EXPECT_TRUE(saw_mr);

    // Add it.  Now `available_to` should NOT include Mesh Renderer.
    r.add_component<MeshRendererComponent>(e, MeshRendererComponent{});
    auto avail2 = reg.available_to(r, static_cast<u64>(e), "Mesh Renderer");
    bool still_saw_mr = false;
    for (auto* d : avail2) {
        if (d->type_id == "Mesh Renderer") still_saw_mr = true;
    }
    EXPECT_FALSE(still_saw_mr);
}

// ── add() actually attaches the component via Registry ──────────────────────

TEST(ComponentRegistry, AddDescriptorAttachesComponentToEntity) {
    ComponentRegistry reg;
    register_builtin_components(reg);

    Scene scene;
    Entity e = scene.create_entity_3d("Box");
    auto& r = scene.registry();
    ASSERT_FALSE(r.has_component<RigidBody3DComponent>(e));

    const auto* d = reg.find("Rigid Body 3D");
    ASSERT_NE(d, nullptr);
    d->add(r, static_cast<u64>(e));
    EXPECT_TRUE(r.has_component<RigidBody3DComponent>(e));
}

// ── add() is idempotent (no double-add corruption) ──────────────────────────

TEST(ComponentRegistry, AddIsIdempotent) {
    ComponentRegistry reg;
    register_builtin_components(reg);

    Scene scene;
    Entity e = scene.create_entity_3d("Box");
    auto& r = scene.registry();
    const auto* d = reg.find("Mesh Renderer");
    ASSERT_NE(d, nullptr);

    d->add(r, static_cast<u64>(e));
    EXPECT_TRUE(r.has_component<MeshRendererComponent>(e));
    // Calling add again must not throw nor invalidate the existing component.
    d->add(r, static_cast<u64>(e));
    EXPECT_TRUE(r.has_component<MeshRendererComponent>(e));
}

// ── remove() detaches; idempotent on missing ────────────────────────────────

TEST(ComponentRegistry, RemoveDescriptorDetachesAndIsIdempotent) {
    ComponentRegistry reg;
    register_builtin_components(reg);

    Scene scene;
    Entity e = scene.create_entity_3d("Box");
    auto& r = scene.registry();
    const auto* d = reg.find("Mesh Renderer");
    ASSERT_NE(d, nullptr);

    d->add(r, static_cast<u64>(e));
    EXPECT_TRUE(r.has_component<MeshRendererComponent>(e));
    d->remove(r, static_cast<u64>(e));
    EXPECT_FALSE(r.has_component<MeshRendererComponent>(e));
    // Idempotent: removing again must not throw.
    d->remove(r, static_cast<u64>(e));
    EXPECT_FALSE(r.has_component<MeshRendererComponent>(e));
}

// ── Custom descriptor — non-builtin user component ─────────────────────────

namespace {
struct UserMarkerComponent {
    int payload{0};
};
}  // namespace

TEST(ComponentRegistry, CustomTypeRegistrationWorks) {
    ComponentRegistry reg;
    reg.register_type<UserMarkerComponent>("User Marker", "Custom");
    EXPECT_EQ(reg.size(), 1u);
    const auto* d = reg.find("User Marker");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->category, "Custom");

    Scene scene;
    Entity e = scene.create_entity_3d("X");
    auto& r = scene.registry();
    EXPECT_FALSE(d->has(r, static_cast<u64>(e)));
    d->add(r, static_cast<u64>(e));
    EXPECT_TRUE(d->has(r, static_cast<u64>(e)));
    EXPECT_TRUE(r.has_component<UserMarkerComponent>(e));
}

// ── available_to with empty entity list (no targets) ───────────────────────

TEST(ComponentRegistry, AvailableToWithMissingEntityIsEmpty) {
    // Behavior contract: when the query passed to `matching()` is empty AND
    // we run available_to() on a non-existent entity, every descriptor
    // returns has() == false (component absent), so all descriptors should
    // remain available.  This guards against silent skipping when the
    // selection is empty.
    ComponentRegistry reg;
    register_builtin_components(reg);
    Scene scene;
    auto& r = scene.registry();
    auto avail = reg.available_to(r, static_cast<u64>(0xFFFFFFFFu), "");
    EXPECT_EQ(avail.size(), reg.size());
}

// ── Categories preserve insertion order ────────────────────────────────────

TEST(ComponentRegistry, CategoriesAreUniqueAndInInsertionOrder) {
    ComponentRegistry reg;
    reg.register_type<UserMarkerComponent>("A", "Cat1");
    reg.register_type<UserMarkerComponent>("B", "Cat2");
    reg.register_type<UserMarkerComponent>("C", "Cat1");  // duplicate cat
    const auto cats = reg.categories();
    ASSERT_EQ(cats.size(), 2u);
    EXPECT_EQ(cats[0], "Cat1");
    EXPECT_EQ(cats[1], "Cat2");
}

// ── Built-ins added in M10 / M16 ─────────────────────────────────────────────
//
// AnimatorComponent (M10) and ScriptComponent (M16) joined the built-in
// catalog later; these guards keep the menu in sync as further engine
// components are added without an out-of-date "Animation" / "Scripts"
// section silently disappearing.

TEST(ComponentRegistry, AnimatorBuiltinIsRegistered) {
    ComponentRegistry reg;
    register_builtin_components(reg);
    const auto* d = reg.find("Animator");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->category, "Animation");
}

TEST(ComponentRegistry, ScriptBuiltinIsRegistered) {
    ComponentRegistry reg;
    register_builtin_components(reg);
    const auto* d = reg.find("Script");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->category, "Scripts");
}

TEST(ComponentRegistry, ScriptCanBeAddedToEntityViaDescriptor) {
    ComponentRegistry reg;
    register_builtin_components(reg);
    const auto* d = reg.find("Script");
    ASSERT_NE(d, nullptr);

    Scene scene;
    Entity e = scene.create_entity_3d("Player");
    auto& r = scene.registry();
    EXPECT_FALSE(d->has(r, static_cast<u64>(e)));
    d->add(r, static_cast<u64>(e));
    EXPECT_TRUE(d->has(r, static_cast<u64>(e)));
    d->remove(r, static_cast<u64>(e));
    EXPECT_FALSE(d->has(r, static_cast<u64>(e)));
}

}  // namespace nexus::editor::tests
