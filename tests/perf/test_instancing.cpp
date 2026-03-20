#include <nexus/perf/instancing.h>
#include <gtest/gtest.h>

using namespace nexus;

TEST(InstanceBatch, AddAndClear) {
    InstanceBatch batch;
    batch.mesh_id = 1;
    batch.material_id = 2;

    batch.add(Mat4{1.0f});
    batch.add(Mat4{1.0f}, Vec4{1, 0, 0, 1});
    EXPECT_EQ(batch.count(), 2u);

    batch.clear();
    EXPECT_EQ(batch.count(), 0u);
}

TEST(InstanceBatch, InstanceData) {
    InstanceBatch batch;
    batch.add(Mat4{1.0f}, Vec4{0.5f, 0.5f, 0.5f, 1.0f}, 42);

    EXPECT_EQ(batch.instances[0].user_data, 42u);
    EXPECT_FLOAT_EQ(batch.instances[0].color.r, 0.5f);
}

TEST(InstanceManager, SubmitAndBatch) {
    InstanceManager mgr;
    mgr.submit(1, 10, Mat4{1.0f});
    mgr.submit(1, 10, Mat4{2.0f}); // same mesh + material
    mgr.submit(2, 10, Mat4{1.0f}); // different mesh

    EXPECT_EQ(mgr.batch_count(), 2u);
    EXPECT_EQ(mgr.total_instances(), 3u);
}

TEST(InstanceManager, Clear) {
    InstanceManager mgr;
    mgr.submit(1, 1, Mat4{1.0f});
    mgr.clear();
    EXPECT_EQ(mgr.batch_count(), 0u);
    EXPECT_EQ(mgr.total_instances(), 0u);
}

TEST(InstanceManager, SplitByThreshold) {
    InstanceManager mgr;
    mgr.set_instance_threshold(3);
    EXPECT_EQ(mgr.instance_threshold(), 3u);

    // Batch with 5 instances (instanced).
    for (int i = 0; i < 5; ++i) {
        mgr.submit(1, 1, Mat4{1.0f});
    }
    // Batch with 1 instance (individual).
    mgr.submit(2, 2, Mat4{1.0f});

    auto split = mgr.split();
    EXPECT_EQ(split.instanced.size(), 1u);
    EXPECT_EQ(split.individual.size(), 1u);
    EXPECT_EQ(split.instanced[0]->count(), 5u);
    EXPECT_EQ(split.individual[0]->count(), 1u);
}

TEST(InstanceManager, DefaultThreshold) {
    InstanceManager mgr;
    EXPECT_EQ(mgr.instance_threshold(), 2u);
}

TEST(InstanceManager, BatchesAccessible) {
    InstanceManager mgr;
    mgr.submit(1, 1, Mat4{1.0f});
    mgr.submit(1, 1, Mat4{2.0f});

    const auto& batches = mgr.batches();
    EXPECT_EQ(batches.size(), 1u);

    auto it = batches.begin();
    EXPECT_EQ(it->second.mesh_id, 1u);
    EXPECT_EQ(it->second.material_id, 1u);
    EXPECT_EQ(it->second.count(), 2u);
}

TEST(InstanceManager, DifferentMaterials) {
    InstanceManager mgr;
    mgr.submit(1, 1, Mat4{1.0f});
    mgr.submit(1, 2, Mat4{1.0f});
    mgr.submit(1, 3, Mat4{1.0f});

    EXPECT_EQ(mgr.batch_count(), 3u);
    EXPECT_EQ(mgr.total_instances(), 3u);
}

TEST(BatchKey, Equality) {
    BatchKey a{1, 2};
    BatchKey b{1, 2};
    BatchKey c{1, 3};

    EXPECT_EQ(a, b);
    EXPECT_FALSE(a == c);
}
