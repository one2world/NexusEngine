#include <gtest/gtest.h>
#include <glm/glm.hpp>

namespace nexus::tests {

TEST(MathTypes, Vec3DefaultConstruction) {
    glm::vec3 v{};
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(MathTypes, Vec3Addition) {
    glm::vec3 a{1.0f, 2.0f, 3.0f};
    glm::vec3 b{4.0f, 5.0f, 6.0f};
    glm::vec3 c = a + b;
    EXPECT_FLOAT_EQ(c.x, 5.0f);
    EXPECT_FLOAT_EQ(c.y, 7.0f);
    EXPECT_FLOAT_EQ(c.z, 9.0f);
}

TEST(MathTypes, Mat4Identity) {
    glm::mat4 m{1.0f};
    EXPECT_FLOAT_EQ(m[0][0], 1.0f);
    EXPECT_FLOAT_EQ(m[1][1], 1.0f);
    EXPECT_FLOAT_EQ(m[2][2], 1.0f);
    EXPECT_FLOAT_EQ(m[3][3], 1.0f);
    EXPECT_FLOAT_EQ(m[0][1], 0.0f);
}

} // namespace nexus::tests
