#include "TrackerConfig.h"

#include <string.h>

TrackerConfig trackerDefaults() {
  TrackerConfig c;
  memset(&c, 0, sizeof(c));
  c.enabled = true;
  c.channel = 1;
  c.ack = false;
  c.advert_location = false;
  memcpy(c.role, "veh", 4);
  c.motion.move_speed_mps = 1.5f;
  c.motion.move_distance_m = 20.0f;
  c.motion.stationary_hold_s = 30;
  c.motion.move_interval_s = 15;
  c.motion.stationary_interval_s = 300;
  c.motion.stale_multiplier = 2.0f;
  c.motion.stale_min_s = 10;
  c.motion.speed_samples = 2;
  return c;
}
