#include <gtest/gtest.h>
#include <nexus/scene/registry.h>
#include <nexus/scene/hierarchy.h>
#include <nexus/scene/components.h>

namespace nexus::tests {

TEST(Hierarchy, SetParent) {
    Registry reg;
    Entity parent = reg.create();
    Entity child = reg.create();

    Hierarchy::set_parent(reg, child, parent);

    EXPECT_EQ(Hierarchy::get_parent(reg, child), parent);
    auto children = Hierarchy::get_children(reg, parent);
    ASSERT_EQ(children.size(), 1u);
    EXPECT_EQ(children[0], child);
}

TEST(Hierarchy, MultipleChildren) {
    Registry reg;
    Entity parent = reg.create();
    Entity c1 = reg.create();
    Entity c2 = reg.create();
    Entity c3 = reg.create();

    Hierarchy::set_parent(reg, c1, parent);
    Hierarchy::set_parent(reg, c2, parent);
    Hierarchy::set_parent(reg, c3, parent);

    auto children = Hierarchy::get_children(reg, parent);
    EXPECT_EQ(children.size(), 3u);
}

TEST(Hierarchy, RemoveParent) {
    Registry reg;
    Entity parent = reg.create();
    Entity child = reg.create();

    Hierarchy::set_parent(reg, child, parent);
    Hierarchy::remove_parent(reg, child);

    EXPECT_EQ(Hierarchy::get_parent(reg, child), INVALID_ENTITY);
    auto children = Hierarchy::get_children(reg, parent);
    EXPECT_EQ(children.size(), 0u);
}

TEST(Hierarchy, PreventSelfParenting) {
    Registry reg;
    Entity e = reg.create();
    Hierarchy::set_parent(reg, e, e);
    EXPECT_EQ(Hierarchy::get_parent(reg, e), INVALID_ENTITY);
}

TEST(Hierarchy, PreventCycles) {
    Registry reg;
    Entity a = reg.create();
    Entity b = reg.create();
    Entity c = reg.create();

    Hierarchy::set_parent(reg, b, a);
    Hierarchy::set_parent(reg, c, b);

    // Trying to make a -> c (which is a descendant of a) should fail
    Hierarchy::set_parent(reg, a, c);
    // a should still not have c as parent
    EXPECT_EQ(Hierarchy::get_parent(reg, a), INVALID_ENTITY);
}

TEST(Hierarchy, GetDescendants) {
    Registry reg;
    Entity root = reg.create();
    Entity child = reg.create();
    Entity grandchild = reg.create();

    Hierarchy::set_parent(reg, child, root);
    Hierarchy::set_parent(reg, grandchild, child);

    auto descendants = Hierarchy::get_descendants(reg, root);
    EXPECT_EQ(descendants.size(), 2u);
}

TEST(Hierarchy, IsAncestor) {
    Registry reg;
    Entity a = reg.create();
    Entity b = reg.create();
    Entity c = reg.create();

    Hierarchy::set_parent(reg, b, a);
    Hierarchy::set_parent(reg, c, b);

    EXPECT_TRUE(Hierarchy::is_ancestor(reg, c, a));
    EXPECT_TRUE(Hierarchy::is_ancestor(reg, b, a));
    EXPECT_FALSE(Hierarchy::is_ancestor(reg, a, c));
}

TEST(Hierarchy, ReparentChild) {
    Registry reg;
    Entity p1 = reg.create();
    Entity p2 = reg.create();
    Entity child = reg.create();

    Hierarchy::set_parent(reg, child, p1);
    EXPECT_EQ(Hierarchy::get_children(reg, p1).size(), 1u);

    // Reparent to p2
    Hierarchy::set_parent(reg, child, p2);
    EXPECT_EQ(Hierarchy::get_children(reg, p1).size(), 0u);
    EXPECT_EQ(Hierarchy::get_children(reg, p2).size(), 1u);
    EXPECT_EQ(Hierarchy::get_parent(reg, child), p2);
}

} // namespace nexus::tests
