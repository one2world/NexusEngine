#include <gtest/gtest.h>
#include <nexus/renderer/material.h>

namespace nexus::tests {

TEST(Material, SetAndGetFloat) {
    Material mat;
    mat.set_float("u_Roughness", 0.5f);

    auto* val = mat.get<float>("u_Roughness");
    ASSERT_NE(val, nullptr);
    EXPECT_FLOAT_EQ(*val, 0.5f);
}

TEST(Material, SetAndGetVec3) {
    Material mat;
    mat.set_vec3("u_Color", Vec3(1.0f, 0.5f, 0.25f));

    auto* val = mat.get<Vec3>("u_Color");
    ASSERT_NE(val, nullptr);
    EXPECT_FLOAT_EQ(val->r, 1.0f);
    EXPECT_FLOAT_EQ(val->g, 0.5f);
    EXPECT_FLOAT_EQ(val->b, 0.25f);
}

TEST(Material, SetAndGetVec4) {
    Material mat;
    mat.set_vec4("u_Tint", Vec4(1.0f, 0.0f, 0.0f, 0.8f));

    auto* val = mat.get<Vec4>("u_Tint");
    ASSERT_NE(val, nullptr);
    EXPECT_FLOAT_EQ(val->a, 0.8f);
}

TEST(Material, GetNonexistentReturnsNull) {
    Material mat;
    EXPECT_EQ(mat.get<float>("u_Nonexistent"), nullptr);
}

TEST(Material, OverwriteProperty) {
    Material mat;
    mat.set_float("u_Value", 1.0f);
    mat.set_float("u_Value", 2.0f);

    auto* val = mat.get<float>("u_Value");
    ASSERT_NE(val, nullptr);
    EXPECT_FLOAT_EQ(*val, 2.0f);
}

TEST(MaterialLibrary, AddAndRetrieve) {
    MaterialLibrary lib;
    Material mat(nexus::rhi::INVALID_HANDLE, "TestMat");
    mat.set_float("u_Value", 42.0f);

    lib.add("test", std::move(mat));

    EXPECT_TRUE(lib.has("test"));
    auto* retrieved = lib.get("test");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name(), "TestMat");
}

TEST(MaterialLibrary, RemoveMaterial) {
    MaterialLibrary lib;
    lib.add("mat", Material(nexus::rhi::INVALID_HANDLE, "Mat"));
    EXPECT_TRUE(lib.has("mat"));
    lib.remove("mat");
    EXPECT_FALSE(lib.has("mat"));
}

} // namespace nexus::tests
