#pragma once

#include <stdint.h>

// Board-agnostic tracker settings. Pins, the radio chip, and the PMU stay in the MeshCore variant.

struct TrackerMotionConfig {
  float move_speed_mps;
  float move_distance_m;
  uint16_t stationary_hold_s;
  uint16_t move_interval_s;
  uint16_t stationary_interval_s;
  float stale_multiplier;
  uint16_t stale_min_s;
  uint8_t speed_samples;  // consecutive speed-only hits required before MOVING
};

struct TrackerConfig {
  bool enabled;
  uint8_t channel;  // private MeshCore channel slot
  bool ack;         // application ACK; off avoids a second flood per fix
  bool advert_location;  // must stay false: live GPS is only inside !MT1
  char role[5];     // k= value, 2-4 letters or digits
  TrackerMotionConfig motion;
};

TrackerConfig trackerDefaults();
