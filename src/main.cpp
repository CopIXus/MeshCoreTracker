#include <Arduino.h>

#include "TrackerConfig.h"
#include "TrackerMessage.h"

// This image proves the board-agnostic core builds on a T-Beam.
// GPS, the radio, and the private channel are wired per docs/PORTING.md.
// TRACKER_BOARD_TBEAM_SX1262 or TRACKER_BOARD_TBEAM_SX1276 selects the test radio.

void setup() {
  Serial.begin(115200);
  delay(300);
  TrackerConfig cfg = trackerDefaults();
  Serial.println("MeshCoreTracker core");
#if defined(TRACKER_BOARD_TBEAM_SX1262)
  Serial.println("board T-Beam SX1262 (test radio)");
#elif defined(TRACKER_BOARD_TBEAM_SX1276)
  Serial.println("board T-Beam SX1276 (test radio)");
#else
  Serial.println("board unset");
#endif
  Serial.printf("role=%s moving=%us still=%us advert_location=%s\n", cfg.role,
                (unsigned)cfg.motion.move_interval_s, (unsigned)cfg.motion.stationary_interval_s,
                cfg.advert_location ? "on" : "off");
  Serial.println("Next step: docs/PORTING.md");
}

void loop() {}
