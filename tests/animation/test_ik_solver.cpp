#include <gtest/gtest.h>
#include <nexus/animation/ik_solver.h>
#include <cmath>

namespace nexus::anim::tests {

TEST(TwoBoneIK, SolveReachableTarget) {
    // Shoulder -> Elbow -> Hand, all along X axis
    Vec3 root(0, 0, 0);
    Vec3 mid(2, 0, 0);
    Vec3 end(4, 0, 0);
    Vec3 target(3, 2, 0);
    Vec3 pole(0, 0, -1); // bend backward

    auto result = TwoBoneIK::solve(root, mid, end, target, pole);
    // Target is within reach (distance from root = sqrt(9+4) ≈ 3.6, max reach = 4)
    EXPECT_TRUE(result.reached);
}

TEST(TwoBoneIK, UnreachableTargetClamps) {
    Vec3 root(0, 0, 0);
    Vec3 mid(1, 0, 0);
    Vec3 end(2, 0, 0);
    Vec3 target(100, 0, 0); // way too far
    Vec3 pole(0, 1, 0);

    auto result = TwoBoneIK::solve(root, mid, end, target, pole);
    EXPECT_FALSE(result.reached);
}

TEST(FABRIKSolver, SolveReachableTarget) {
    FABRIKSolver solver;
    std::vector<Vec3> chain = {
        {0, 0, 0},
        {1, 0, 0},
        {2, 0, 0},
        {3, 0, 0}
    };
    solver.set_chain(chain);
    solver.set_tolerance(0.01f);
    solver.set_max_iterations(20);

    Vec3 target(2, 2, 0);
    solver.solve(target);

    // End effector should be close to target
    const auto& positions = solver.positions();
    float dist = glm::length(positions.back() - target);
    EXPECT_LT(dist, 0.1f); // within tolerance
}

TEST(FABRIKSolver, UnreachableTarget) {
    FABRIKSolver solver;
    std::vector<Vec3> chain = {
        {0, 0, 0},
        {1, 0, 0},
        {2, 0, 0}
    };
    solver.set_chain(chain);

    Vec3 target(100, 0, 0); // total chain length = 2, target at 100
    bool reached = solver.solve(target);
    EXPECT_FALSE(reached);

    // Chain should be fully extended toward target
    const auto& positions = solver.positions();
    EXPECT_NEAR(positions.back().x, 2.0f, 0.01f);
}

TEST(FABRIKSolver, SingleSegment) {
    FABRIKSolver solver;
    std::vector<Vec3> chain = {{0, 0, 0}, {1, 0, 0}};
    solver.set_chain(chain);

    // Target at exact distance 1 from origin along a different direction
    Vec3 target(0, 1, 0);
    solver.solve(target);

    const auto& pos = solver.positions();
    float dist = glm::length(pos.back() - target);
    EXPECT_LT(dist, 0.1f);
}

TEST(FABRIKSolver, PreservesChainLengths) {
    FABRIKSolver solver;
    std::vector<Vec3> chain = {
        {0, 0, 0},
        {1, 0, 0},
        {2, 0, 0}
    };
    solver.set_chain(chain);
    solver.solve({1.5f, 1.0f, 0});

    const auto& pos = solver.positions();
    float len0 = glm::length(pos[1] - pos[0]);
    float len1 = glm::length(pos[2] - pos[1]);
    EXPECT_NEAR(len0, 1.0f, 0.01f);
    EXPECT_NEAR(len1, 1.0f, 0.01f);
}

} // namespace nexus::anim::tests
