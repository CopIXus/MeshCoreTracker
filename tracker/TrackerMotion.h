#pragma once

#include "TrackerConfig.h"

enum class TrackerMotionState : uint8_t { Seek, Stationary, Moving };

struct TrackerFix {
  bool valid;
  double lat;
  double lon;
  float speed_mps;
};

struct TrackerDecision {
  bool send;
  TrackerMotionState state;
  uint16_t interval_s;
  uint16_t stale_s;
};

class TrackerMotion {
public:
  TrackerMotion();
  void reset();
  void setConfig(const TrackerMotionConfig& cfg);
  const TrackerMotionConfig& config() const { return _cfg; }
  // now_s is a monotonic second counter (millis()/1000 is fine). GPS time is not required.
  TrackerDecision onFix(const TrackerFix& fix, uint32_t now_s);

private:
  TrackerMotionConfig _cfg;
  TrackerMotionState _state;
  bool _have_anchor;
  double _anchor_lat;
  double _anchor_lon;
  double _motion_lat;
  double _motion_lon;
  uint32_t _last_motion_s;
  uint32_t _last_sent_s;
  uint8_t _speed_run;
  bool _have_sent;

  uint16_t staleFor(uint16_t interval_s) const;
};
