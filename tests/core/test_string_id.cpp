#include <gtest/gtest.h>
#include <nexus/core/string_id.h>

namespace nexus::tests {

TEST(StringId, SameStringsSameHash) {
    StringId a("hello");
    StringId b("hello");
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.hash(), b.hash());
}

TEST(StringId, DifferentStringsDifferentHash) {
    StringId a("hello");
    StringId b("world");
    EXPECT_NE(a, b);
}

TEST(StringId, EmptyString) {
    StringId empty("");
    StringId non_empty("x");
    EXPECT_NE(empty, non_empty);
}

TEST(StringId, CStringRetrieval) {
    StringId id("test_string");
    EXPECT_STREQ(id.c_str(), "test_string");
}

} // namespace nexus::tests
