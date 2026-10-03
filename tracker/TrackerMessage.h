#pragma once

#include <stddef.h>
#include <stdint.h>

// !MT1 body only. The MeshCore group text already prefixes "CALLSIGN: ".
// Color, icon, and CoT type are not fields. MeshcoreToTAK paints those.

struct TrackerReport {
  char id[9];    // 8 hex chars from the first 4 public-key bytes
  char role[5];  // k=
  double lat;
  double lon;
  float speed_mps;
  float course_deg;
  float altitude_m;
  int battery_pct;  // 0-100, or -1 to omit
  bool has_speed;
  bool has_course;
  bool has_altitude;
  uint16_t stale_sec;
  uint32_t sequence;
};

// 8 uppercase hex chars. pub is the first 4 bytes of the MeshCore public key.
void trackerIdFromPublicKey(const uint8_t pub[4], char out[9]);

// Bytes written, or 0 if the report would be rejected by the gateway or does not fit.
size_t trackerFormatFix(char* dest, size_t dest_len, const TrackerReport& report);
