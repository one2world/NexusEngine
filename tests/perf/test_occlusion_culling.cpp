#include <nexus/perf/occlusion_culling.h>
#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace nexus;

TEST(Frustum, FromViewProjection) {
    Mat4 view = glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    Mat4 vp = proj * view;

    Frustum f = Frustum::from_view_projection(vp);
    // All 6 planes should have non-zero normals.
    for (int i = 0; i < 6; ++i) {
        EXPECT_GT(glm::length(f.planes[i].normal), 0.5f);
    }
}

TEST(Frustum, AABBInsideFrustum) {
    Mat4 view = glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    Frustum f = Frustum::from_view_projection(proj * view);

    CullAABB box{{-1, -1, -1}, {1, 1, 1}};
    EXPECT_TRUE(f.test_aabb(box));
}

TEST(Frustum, AABBOutsideFrustum) {
    Mat4 view = glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 10.0f);
    Frustum f = Frustum::from_view_projection(proj * view);

    // Far behind the camera.
    CullAABB box{{-1, -1, 100}, {1, 1, 102}};
    EXPECT_FALSE(f.test_aabb(box));
}

TEST(Frustum, AABBBehindCamera) {
    Mat4 view = glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 10.0f);
    Frustum f = Frustum::from_view_projection(proj * view);

    CullAABB box{{-1, -1, 8}, {1, 1, 10}};
    EXPECT_FALSE(f.test_aabb(box));
}

TEST(FrustumPlane, SignedDistance) {
    FrustumPlane p;
    p.normal = Vec3(0, 1, 0);
    p.distance = -5.0f;

    EXPECT_GT(p.signed_distance(Vec3(0, 10, 0)), 0.0f);
    EXPECT_LT(p.signed_distance(Vec3(0, 0, 0)), 0.0f);
}

TEST(SoftwareDepthBuffer, Construction) {
    SoftwareDepthBuffer db(64, 64);
    EXPECT_EQ(db.width(), 64u);
    EXPECT_EQ(db.height(), 64u);
    EXPECT_EQ(db.buffer().size(), 64u * 64u);
}

TEST(SoftwareDepthBuffer, ClearSetsMaxDepth) {
    SoftwareDepthBuffer db(8, 8);
    db.clear();
    for (auto d : db.buffer()) {
        EXPECT_FLOAT_EQ(d, 1.0f);
    }
}

TEST(SoftwareDepthBuffer, DefaultEmpty) {
    SoftwareDepthBuffer db;
    EXPECT_EQ(db.width(), 0u);
    EXPECT_EQ(db.height(), 0u);
}

TEST(OcclusionCuller, FrustumCullOnly) {
    Mat4 view = glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);

    OcclusionCuller culler;
    culler.set_view_projection(proj * view);

    std::vector<CullObject> objects;
    objects.push_back({0, {{-1, -1, -1}, {1, 1, 1}}});       // visible
    objects.push_back({1, {{-1, -1, 100}, {1, 1, 102}}});     // behind camera

    auto visible = culler.frustum_cull(objects);
    EXPECT_EQ(visible.size(), 1u);
    EXPECT_EQ(visible[0], 0u);
    EXPECT_EQ(culler.frustum_culled(), 1u);
}

TEST(OcclusionCuller, CullWithOcclusion) {
    Mat4 view = glm::lookAt(Vec3(0, 0, 10), Vec3(0, 0, 0), Vec3(0, 1, 0));
    Mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    Mat4 vp = proj * view;

    OcclusionCuller culler(64, 64);
    culler.set_view_projection(vp);

    // Submit a large occluder right in front of camera.
    CullAABB occluder{{-5, -5, -1}, {5, 5, 1}};
    culler.submit_occluder(occluder);

    // Object behind the occluder.
    CullObject behind;
    behind.id = 1;
    behind.bounds = {{-0.5f, -0.5f, -5.0f}, {0.5f, 0.5f, -4.0f}};

    // Object in front (should be visible).
    CullObject front;
    front.id = 0;
    front.bounds = {{-0.5f, -0.5f, 3.0f}, {0.5f, 0.5f, 4.0f}};

    auto visible = culler.cull({front, behind});
    // Front should definitely be visible.
    bool front_visible = false;
    for (auto id : visible) {
        if (id == 0) front_visible = true;
    }
    EXPECT_TRUE(front_visible);
    EXPECT_EQ(culler.total_tested(), 2u);
}

TEST(OcclusionCuller, Stats) {
    OcclusionCuller culler;
    Mat4 vp = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f) *
              glm::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    culler.set_view_projection(vp);

    std::vector<CullObject> objects;
    for (u32 i = 0; i < 10; ++i) {
        objects.push_back({i, {{-1, -1, -1}, {1, 1, 1}}});
    }
    auto visible = culler.frustum_cull(objects);
    EXPECT_FALSE(visible.empty());
    EXPECT_EQ(culler.total_tested(), 10u);
}

TEST(CullAABB, CenterAndExtents) {
    CullAABB box{{-2, -3, -4}, {2, 3, 4}};
    auto c = box.center();
    EXPECT_FLOAT_EQ(c.x, 0.0f);
    EXPECT_FLOAT_EQ(c.y, 0.0f);
    EXPECT_FLOAT_EQ(c.z, 0.0f);
    auto e = box.extents();
    EXPECT_FLOAT_EQ(e.x, 2.0f);
    EXPECT_FLOAT_EQ(e.y, 3.0f);
    EXPECT_FLOAT_EQ(e.z, 4.0f);
}
