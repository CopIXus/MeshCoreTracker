#include "TrackerMessage.h"

#include <stdio.h>
#include <string.h>

void trackerIdFromPublicKey(const uint8_t pub[4], char out[9]) {
  static const char* hex = "0123456789ABCDEF";
  for (int i = 0; i < 4; i++) {
    out[i * 2] = hex[pub[i] >> 4];
    out[i * 2 + 1] = hex[pub[i] & 0x0f];
  }
  out[8] = 0;
}

static bool hexId(const char* s) {
  if (!s) return false;
  for (int i = 0; i < 8; i++) {
    char c = s[i];
    bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
    if (!ok) return false;
  }
  return s[8] == 0;
}

static bool roleOk(const char* s) {
  if (!s) return false;
  size_t n = strlen(s);
  if (n < 2 || n > 4) return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z');
    if (!ok) return false;
  }
  return true;
}

static int append(char* dest, size_t dest_len, int used, const char* fmt, double a) {
  if (used < 0 || (size_t)used >= dest_len) return -1;
  int n = snprintf(dest + used, dest_len - (size_t)used, fmt, a);
  if (n < 0 || (size_t)used + (size_t)n >= dest_len) return -1;
  return used + n;
}

static int appendU(char* dest, size_t dest_len, int used, const char* fmt, unsigned long a) {
  if (used < 0 || (size_t)used >= dest_len) return -1;
  int n = snprintf(dest + used, dest_len - (size_t)used, fmt, a);
  if (n < 0 || (size_t)used + (size_t)n >= dest_len) return -1;
  return used + n;
}

size_t trackerFormatFix(char* dest, size_t dest_len, const TrackerReport& report) {
  if (!dest || dest_len < 16) return 0;
  if (!hexId(report.id) || !roleOk(report.role)) return 0;
  if (report.lat < -90.0 || report.lat > 90.0 || report.lon < -180.0 || report.lon > 180.0) return 0;
  if (report.lat == 0.0 && report.lon == 0.0) return 0;
  if (report.stale_sec < 5 || report.stale_sec > 3600) return 0;
  if (report.has_speed && (report.speed_mps < 0.0f || report.speed_mps > 200.0f)) return 0;
  if (report.has_course && (report.course_deg < 0.0f || report.course_deg >= 360.0f)) return 0;
  if (report.has_altitude && (report.altitude_m < -1000.0f || report.altitude_m > 20000.0f)) return 0;
  if (report.battery_pct < -1 || report.battery_pct > 100) return 0;

  int n = snprintf(dest, dest_len, "!MT1;u=%s;k=%s;la=%.6f;ln=%.6f", report.id, report.role, report.lat,
                   report.lon);
  if (n < 0 || (size_t)n >= dest_len) return 0;
  if (report.has_speed) n = append(dest, dest_len, n, ";s=%.1f", report.speed_mps);
  if (report.has_course) n = append(dest, dest_len, n, ";c=%.0f", report.course_deg);
  if (report.has_altitude) n = append(dest, dest_len, n, ";a=%.0f", report.altitude_m);
  n = appendU(dest, dest_len, n, ";st=%lu", report.stale_sec);
  n = appendU(dest, dest_len, n, ";q=%lu", report.sequence);
  if (report.battery_pct >= 0) n = append(dest, dest_len, n, ";b=%.0f", report.battery_pct);
  if (n < 0) return 0;
  // Leave room for "CALLSIGN: " inside MeshCore's 160-byte group text.
  if ((size_t)n > 120) return 0;
  return (size_t)n;
}
