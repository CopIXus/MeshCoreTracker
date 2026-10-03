#include <string.h>

#include <gtest/gtest.h>

#include "TrackerConfig.h"
#include "TrackerMessage.h"
#include "TrackerMotion.h"

static TrackerFix fixAt(double lat, double lon, float speed) {
  TrackerFix f;
  f.valid = true;
  f.lat = lat;
  f.lon = lon;
  f.speed_mps = speed;
  return f;
}

// Matches TrackerMotion's 6371000 m sphere.
static double northOf(double lat, double meters) {
  return lat + meters / 111194.92664455873;
}

TEST(TrackerMessage, IdAndCanonicalFix) {
  uint8_t pub[4] = {0xA1, 0xB2, 0xC3, 0xD4};
  char id[9];
  trackerIdFromPublicKey(pub, id);
  EXPECT_STREQ("A1B2C3D4", id);

  TrackerReport r;
  memset(&r, 0, sizeof(r));
  memcpy(r.id, id, 9);
  memcpy(r.role, "k9", 3);
  r.lat = 36.123456;
  r.lon = -82.123456;
  r.has_speed = true;
  r.speed_mps = 1.2f;
  r.has_course = true;
  r.course_deg = 90;
  r.has_altitude = true;
  r.altitude_m = 512;
  r.battery_pct = 87;
  r.stale_sec = 30;
  r.sequence = 1042;
  char buf[160];
  size_t n = trackerFormatFix(buf, sizeof(buf), r);
  ASSERT_GT(n, 0u);
  EXPECT_LE(n, 120u);
  EXPECT_STREQ("!MT1;u=A1B2C3D4;k=k9;la=36.123456;ln=-82.123456;s=1.2;c=90;a=512;st=30;q=1042;b=87", buf);
}

TEST(TrackerMessage, RejectsBogusFix) {
  TrackerReport r;
  memset(&r, 0, sizeof(r));
  memcpy(r.id, "A1B2C3D4", 9);
  memcpy(r.role, "veh", 4);
  r.lat = 0;
  r.lon = 0;
  r.battery_pct = -1;
  r.stale_sec = 30;
  r.sequence = 1;
  char buf[160];
  EXPECT_EQ(0u, trackerFormatFix(buf, sizeof(buf), r));
  r.lat = 36.1;
  r.lon = -82.1;
  r.stale_sec = 0;
  EXPECT_EQ(0u, trackerFormatFix(buf, sizeof(buf), r));
}

TEST(TrackerMotion, FirstFixSendsAndJitterStaysStill) {
  TrackerMotion m;
  TrackerDecision d = m.onFix(fixAt(36.0, -82.0, 0.2f), 0);
  EXPECT_TRUE(d.send);
  EXPECT_EQ(TrackerMotionState::Stationary, d.state);
  EXPECT_EQ(300, d.interval_s);
  EXPECT_EQ(600, d.stale_s);

  d = m.onFix(fixAt(northOf(36.0, 8), -82.0, 0.4f), 5);
  EXPECT_FALSE(d.send);
  EXPECT_EQ(TrackerMotionState::Stationary, d.state);
}

TEST(TrackerMotion, DisplacementAndSpeed) {
  TrackerMotion m;
  m.onFix(fixAt(36.0, -82.0, 0), 0);
  TrackerDecision d = m.onFix(fixAt(northOf(36.0, 25), -82.0, 0), 2);
  EXPECT_TRUE(d.send);
  EXPECT_EQ(TrackerMotionState::Moving, d.state);
  EXPECT_EQ(15, d.interval_s);
  EXPECT_EQ(30, d.stale_s);

  TrackerMotion speed;
  speed.onFix(fixAt(36.0, -82.0, 0), 0);
  d = speed.onFix(fixAt(36.0, -82.0, 2.0f), 1);
  EXPECT_FALSE(d.send);
  EXPECT_EQ(TrackerMotionState::Stationary, d.state);
  d = speed.onFix(fixAt(36.0, -82.0, 2.0f), 2);
  EXPECT_TRUE(d.send);
  EXPECT_EQ(TrackerMotionState::Moving, d.state);
}

TEST(TrackerMotion, StoplightHoldThenStationary) {
  TrackerMotion m;
  m.onFix(fixAt(36.0, -82.0, 0), 0);
  m.onFix(fixAt(northOf(36.0, 25), -82.0, 3.0f), 1);
  TrackerDecision d = m.onFix(fixAt(northOf(36.0, 25), -82.0, 0), 10);
  EXPECT_EQ(TrackerMotionState::Moving, d.state);
  EXPECT_FALSE(d.send);
  d = m.onFix(fixAt(northOf(36.0, 25), -82.0, 0), 31);
  EXPECT_TRUE(d.send);
  EXPECT_EQ(TrackerMotionState::Stationary, d.state);
  EXPECT_EQ(600, d.stale_s);
}

TEST(TrackerMotion, InvalidFixDoesNotMoveTheMarker) {
  TrackerMotion m;
  m.onFix(fixAt(36.0, -82.0, 0), 0);
  TrackerFix bad;
  bad.valid = false;
  bad.lat = 0;
  bad.lon = 0;
  bad.speed_mps = 0;
  TrackerDecision d = m.onFix(bad, 10);
  EXPECT_FALSE(d.send);
  EXPECT_EQ(TrackerMotionState::Stationary, d.state);
}

TEST(TrackerConfig, DefaultsStayOffTheAdvert) {
  TrackerConfig c = trackerDefaults();
  EXPECT_FALSE(c.advert_location);
  EXPECT_FALSE(c.ack);
  EXPECT_EQ(15, c.motion.move_interval_s);
  EXPECT_EQ(300, c.motion.stationary_interval_s);
  EXPECT_STREQ("veh", c.role);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
