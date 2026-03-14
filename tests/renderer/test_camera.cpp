#include <gtest/gtest.h>
#include <nexus/renderer/camera.h>

namespace nexus::tests {

TEST(Camera2D, DefaultValues) {
    nexus::Camera2D cam;
    EXPECT_FLOAT_EQ(cam.position.x, 0.0f);
    EXPECT_FLOAT_EQ(cam.position.y, 0.0f);
    EXPECT_FLOAT_EQ(cam.zoom, 1.0f);
    EXPECT_FLOAT_EQ(cam.rotation, 0.0f);
}

TEST(Camera2D, ViewProjectionNotIdentity) {
    nexus::Camera2D cam;
    cam.set_projection(1280.0f, 720.0f);
    nexus::Mat4 vp = cam.get_view_projection();
    // Should not be identity since projection is set
    EXPECT_NE(vp[0][0], 1.0f);
}

TEST(Camera3D, DefaultForward) {
    nexus::Camera3D cam;
    // Default yaw is -90, pitch is 0 -> forward should be roughly (0, 0, -1)
    nexus::Vec3 fwd = cam.forward();
    EXPECT_NEAR(fwd.x, 0.0f, 0.01f);
    EXPECT_NEAR(fwd.y, 0.0f, 0.01f);
    EXPECT_NEAR(fwd.z, -1.0f, 0.01f);
}

TEST(Camera3D, LookAt) {
    nexus::Camera3D cam;
    cam.position = nexus::Vec3(0.0f, 0.0f, 5.0f);
    cam.look_at(nexus::Vec3(0.0f, 0.0f, 0.0f));

    nexus::Vec3 fwd = cam.forward();
    // Should point in -Z direction
    EXPECT_NEAR(fwd.z, -1.0f, 0.1f);
}

TEST(Camera3D, PerspectiveProjection) {
    nexus::Camera3D cam;
    cam.set_perspective(16.0f / 9.0f);
    nexus::Mat4 proj = cam.get_projection_matrix();
    // Perspective matrix should have non-zero diagonal
    EXPECT_NE(proj[0][0], 0.0f);
    EXPECT_NE(proj[1][1], 0.0f);
}

} // namespace nexus::tests
