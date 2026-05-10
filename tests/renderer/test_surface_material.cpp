// SurfaceMaterial — strong-typed material struct that ForwardRenderer3D
// pushes into u_Material_* uniforms each draw.  These tests validate the
// defaults match the documented neutral-surface preset and that the
// non-shader-touching contract is honoured (defaults() round-trips,
// fields are independently settable).

#include <gtest/gtest.h>
#include <nexus/renderer/surface_material.h>

using namespace nexus;

TEST(SurfaceMaterial, DefaultIsNeutralWhiteSurface) {
    SurfaceMaterial m = SurfaceMaterial::defaults();
    // Neutral white albedo so unset entities render with their tint
    // (the legacy MeshRendererComponent.tint multiplier).
    EXPECT_FLOAT_EQ(m.albedo.r, 1.0f);
    EXPECT_FLOAT_EQ(m.albedo.g, 1.0f);
    EXPECT_FLOAT_EQ(m.albedo.b, 1.0f);
    EXPECT_FLOAT_EQ(m.albedo.a, 1.0f);
    // White specular, mid weight, conventional Blinn-Phong shininess.
    EXPECT_FLOAT_EQ(m.specular_color.r, 1.0f);
    EXPECT_FLOAT_EQ(m.specular_color.g, 1.0f);
    EXPECT_FLOAT_EQ(m.specular_color.b, 1.0f);
    EXPECT_FLOAT_EQ(m.specular_strength, 0.5f);
    EXPECT_FLOAT_EQ(m.shininess, 32.0f);
    // Vanilla Lambert: no wrap by default.  Materials that want soft
    // terminators opt in explicitly so a "polished plastic" preset
    // can sit next to a "matte chalk" preset without one bleeding
    // its softness across the scene.
    EXPECT_FLOAT_EQ(m.diffuse_wrap, 0.0f);
    // Full ambient response — surfaces accept the scene fill light
    // unless a host explicitly dims them down.
    EXPECT_FLOAT_EQ(m.ambient_response, 1.0f);
    // No emissive — surfaces don't self-illuminate by default.
    EXPECT_FLOAT_EQ(m.emissive.r, 0.0f);
    EXPECT_FLOAT_EQ(m.emissive.g, 0.0f);
    EXPECT_FLOAT_EQ(m.emissive.b, 0.0f);
    EXPECT_FLOAT_EQ(m.emissive_strength, 0.0f);
    // PBR fields match common engine defaults so a future PBR shader
    // can adopt them without changing existing materials.
    EXPECT_FLOAT_EQ(m.metallic, 0.0f);
    EXPECT_FLOAT_EQ(m.roughness, 1.0f);
}

TEST(SurfaceMaterial, FieldsAreIndependentlySettable) {
    SurfaceMaterial a = SurfaceMaterial::defaults();
    SurfaceMaterial b = SurfaceMaterial::defaults();
    // Tweak only b — a must stay default.
    b.albedo            = Vec4(0.2f, 0.6f, 0.9f, 1.0f);
    b.specular_strength = 0.9f;
    b.shininess         = 96.0f;
    b.diffuse_wrap      = 0.3f;
    b.emissive          = Vec3(0.1f, 0.0f, 0.0f);
    b.emissive_strength = 2.0f;

    EXPECT_FLOAT_EQ(a.albedo.r,           1.0f);
    EXPECT_FLOAT_EQ(a.specular_strength,  0.5f);
    EXPECT_FLOAT_EQ(a.shininess,          32.0f);
    EXPECT_FLOAT_EQ(a.diffuse_wrap,       0.0f);
    EXPECT_FLOAT_EQ(a.emissive.r,         0.0f);
    EXPECT_FLOAT_EQ(a.emissive_strength,  0.0f);

    EXPECT_FLOAT_EQ(b.albedo.r,           0.2f);
    EXPECT_FLOAT_EQ(b.specular_strength,  0.9f);
    EXPECT_FLOAT_EQ(b.shininess,          96.0f);
    EXPECT_FLOAT_EQ(b.diffuse_wrap,       0.3f);
    EXPECT_FLOAT_EQ(b.emissive.r,         0.1f);
    EXPECT_FLOAT_EQ(b.emissive_strength,  2.0f);
}

TEST(SurfaceMaterial, IsTriviallyCopyable) {
    // SurfaceMaterial is documented as a flat POD the renderer can
    // store in a hash table without indirection.  Lock that contract:
    // a regression that adds a non-trivial member (vector / unique_ptr)
    // would silently break performance assumptions.
    EXPECT_TRUE(std::is_trivially_copyable_v<SurfaceMaterial>);
}
