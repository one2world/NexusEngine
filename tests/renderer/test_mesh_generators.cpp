#include <gtest/gtest.h>
#include <nexus/renderer/forward_renderer_3d.h>

namespace nexus::tests {

TEST(MeshGenerators, CubeMesh) {
    auto cube = nexus::create_cube_mesh();
    EXPECT_EQ(cube.vertices.size(), 24u); // 6 faces * 4 vertices
    EXPECT_EQ(cube.indices.size(), 36u);  // 6 faces * 6 indices
}

TEST(MeshGenerators, CubeNormalsNormalized) {
    auto cube = nexus::create_cube_mesh();
    for (const auto& v : cube.vertices) {
        float len = glm::length(v.normal);
        EXPECT_NEAR(len, 1.0f, 0.001f);
    }
}

TEST(MeshGenerators, PlaneMesh) {
    auto plane = nexus::create_plane_mesh(10.0f, 2);
    // 2 subdivisions = 3x3 = 9 vertices
    EXPECT_EQ(plane.vertices.size(), 9u);
    // 2x2 = 4 quads * 6 indices = 24
    EXPECT_EQ(plane.indices.size(), 24u);
}

TEST(MeshGenerators, PlaneNormalsPointUp) {
    auto plane = nexus::create_plane_mesh(5.0f, 1);
    for (const auto& v : plane.vertices) {
        EXPECT_FLOAT_EQ(v.normal.x, 0.0f);
        EXPECT_FLOAT_EQ(v.normal.y, 1.0f);
        EXPECT_FLOAT_EQ(v.normal.z, 0.0f);
    }
}

TEST(MeshGenerators, SphereMesh) {
    auto sphere = nexus::create_sphere_mesh(1.0f, 8, 16);
    // (rings+1) * (sectors+1) vertices
    EXPECT_EQ(sphere.vertices.size(), 9u * 17u);
    // rings * sectors * 6 indices
    EXPECT_EQ(sphere.indices.size(), 8u * 16u * 6u);
}

TEST(MeshGenerators, SphereNormalsNormalized) {
    auto sphere = nexus::create_sphere_mesh(1.0f, 8, 16);
    for (const auto& v : sphere.vertices) {
        float len = glm::length(v.normal);
        EXPECT_NEAR(len, 1.0f, 0.01f);
    }
}

TEST(MeshGenerators, SphereVerticesOnSurface) {
    float radius = 2.0f;
    auto sphere = nexus::create_sphere_mesh(radius, 8, 16);
    for (const auto& v : sphere.vertices) {
        float dist = glm::length(v.position);
        EXPECT_NEAR(dist, radius, 0.01f);
    }
}

} // namespace nexus::tests
