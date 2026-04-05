#include <gtest/gtest.h>
#include "nexus/audio/reverb_zone.h"

using namespace nexus;
using namespace nexus::audio;

TEST(ReverbZone, AddAndRemoveZone) {
    ReverbZoneManager mgr;

    ReverbZone zone;
    zone.name = "Cave";
    zone.shape = ReverbZoneShape::AABB;
    zone.center = Vec3(0.0f);
    zone.half_extents = Vec3(5.0f);

    u32 idx = mgr.add_zone(zone);
    EXPECT_EQ(idx, 0u);
    EXPECT_EQ(mgr.zone_count(), 1u);

    mgr.remove_zone(0);
    EXPECT_EQ(mgr.zone_count(), 0u);
}

TEST(ReverbZone, ListenerInsideAABB) {
    ReverbZoneManager mgr;

    ReverbZone zone;
    zone.name = "Room";
    zone.shape = ReverbZoneShape::AABB;
    zone.center = Vec3(0.0f);
    zone.half_extents = Vec3(5.0f);
    zone.config.reverb.room_size = 0.9f;
    zone.config.reverb.wet = 0.5f;
    zone.config.fade_distance = 2.0f;
    mgr.add_zone(zone);

    mgr.update_listener(Vec3(0.0f, 0.0f, 0.0f)); // Inside
    EXPECT_TRUE(mgr.is_active());
    EXPECT_EQ(mgr.active_zone_count(), 1u);
    EXPECT_GT(mgr.zone_weight(0), 0.0f);
}

TEST(ReverbZone, ListenerOutsideAABB) {
    ReverbZoneManager mgr;

    ReverbZone zone;
    zone.name = "Room";
    zone.shape = ReverbZoneShape::AABB;
    zone.center = Vec3(0.0f);
    zone.half_extents = Vec3(5.0f);
    zone.config.fade_distance = 2.0f;
    mgr.add_zone(zone);

    mgr.update_listener(Vec3(100.0f, 0.0f, 0.0f)); // Far outside
    EXPECT_FALSE(mgr.is_active());
}

TEST(ReverbZone, SphereZone) {
    ReverbZoneManager mgr;

    ReverbZone zone;
    zone.name = "Sphere";
    zone.shape = ReverbZoneShape::Sphere;
    zone.center = Vec3(0.0f);
    zone.radius = 10.0f;
    zone.config.fade_distance = 3.0f;
    zone.config.reverb.room_size = 0.7f;
    mgr.add_zone(zone);

    // Inside sphere
    mgr.update_listener(Vec3(0.0f));
    EXPECT_TRUE(mgr.is_active());
    EXPECT_GT(mgr.zone_weight(0), 0.5f);

    // Outside sphere + fade
    mgr.update_listener(Vec3(20.0f, 0.0f, 0.0f));
    EXPECT_FALSE(mgr.is_active());
}

TEST(ReverbZone, BlendingMultipleZones) {
    ReverbZoneManager mgr;

    ReverbZone z1;
    z1.name = "Zone1";
    z1.shape = ReverbZoneShape::AABB;
    z1.center = Vec3(0.0f);
    z1.half_extents = Vec3(10.0f);
    z1.config.reverb.room_size = 0.3f;
    z1.config.reverb.wet = 0.2f;
    z1.config.priority = 0;
    mgr.add_zone(z1);

    ReverbZone z2;
    z2.name = "Zone2";
    z2.shape = ReverbZoneShape::AABB;
    z2.center = Vec3(0.0f);
    z2.half_extents = Vec3(5.0f);
    z2.config.reverb.room_size = 0.9f;
    z2.config.reverb.wet = 0.8f;
    z2.config.priority = 1;
    mgr.add_zone(z2);

    // Listener inside both zones
    mgr.update_listener(Vec3(0.0f));
    EXPECT_EQ(mgr.active_zone_count(), 2u);

    // Blended config should have values between the two
    auto cfg = mgr.current_reverb();
    EXPECT_GT(cfg.room_size, 0.0f);
    EXPECT_LT(cfg.room_size, 1.0f);
}

TEST(ReverbZone, DisabledZone) {
    ReverbZoneManager mgr;

    ReverbZone zone;
    zone.name = "Disabled";
    zone.center = Vec3(0.0f);
    zone.half_extents = Vec3(10.0f);
    zone.enabled = false;
    mgr.add_zone(zone);

    mgr.update_listener(Vec3(0.0f));
    EXPECT_FALSE(mgr.is_active());
}

TEST(ReverbZone, Clear) {
    ReverbZoneManager mgr;

    ReverbZone zone;
    zone.name = "Test";
    mgr.add_zone(zone);
    mgr.add_zone(zone);
    EXPECT_EQ(mgr.zone_count(), 2u);

    mgr.clear();
    EXPECT_EQ(mgr.zone_count(), 0u);
}
