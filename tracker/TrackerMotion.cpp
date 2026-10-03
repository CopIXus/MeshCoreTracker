#include "TrackerMotion.h"

#include <math.h>

TrackerMotion::TrackerMotion() {
  reset();
}

void TrackerMotion::reset() {
  _cfg = trackerDefaults().motion;
  _state = TrackerMotionState::Seek;
  _have_anchor = false;
  _anchor_lat = 0;
  _anchor_lon = 0;
  _motion_lat = 0;
  _motion_lon = 0;
  _last_motion_s = 0;
  _last_sent_s = 0;
  _speed_run = 0;
  _have_sent = false;
}

void TrackerMotion::setConfig(const TrackerMotionConfig& cfg) { _cfg = cfg; }

uint16_t TrackerMotion::staleFor(uint16_t interval_s) const {
  float v = (float)interval_s * _cfg.stale_multiplier;
  if (v < (float)_cfg.stale_min_s) v = (float)_cfg.stale_min_s;
  if (v < 5.0f) v = 5.0f;
  if (v > 3600.0f) v = 3600.0f;
  return (uint16_t)(v + 0.5f);
}

static double distanceM(double lat1, double lon1, double lat2, double lon2) {
  const double rad = 0.017453292519943295;
  const double r = 6371000.0;
  double dlat = (lat2 - lat1) * rad;
  double dlon = (lon2 - lon1) * rad;
  double lat1r = lat1 * rad;
  double lat2r = lat2 * rad;
  double a = sin(dlat / 2) * sin(dlat / 2) + cos(lat1r) * cos(lat2r) * sin(dlon / 2) * sin(dlon / 2);
  return 2 * r * atan2(sqrt(a), sqrt(1 - a));
}

TrackerDecision TrackerMotion::onFix(const TrackerFix& fix, uint32_t now_s) {
  TrackerDecision out;
  out.send = false;
  out.state = _state;
  out.interval_s = _state == TrackerMotionState::Moving ? _cfg.move_interval_s : _cfg.stationary_interval_s;
  out.stale_s = staleFor(out.interval_s);
  if (!fix.valid) return out;

  if (!_have_anchor) {
    _have_anchor = true;
    _anchor_lat = fix.lat;
    _anchor_lon = fix.lon;
    _state = TrackerMotionState::Stationary;
    _last_motion_s = now_s;
    _speed_run = 0;
    bool fast = fix.speed_mps >= _cfg.move_speed_mps;
    if (fast && _cfg.speed_samples <= 1) {
      _state = TrackerMotionState::Moving;
      _speed_run = 1;
    } else if (fast) {
      _speed_run = 1;
    }
    _have_sent = true;
    _last_sent_s = now_s;
    out.send = true;
    out.state = _state;
    out.interval_s = _state == TrackerMotionState::Moving ? _cfg.move_interval_s : _cfg.stationary_interval_s;
    out.stale_s = staleFor(out.interval_s);
    return out;
  }

  // Parked units measure travel from the stationary anchor. Moving units measure
  // travel from the last point that still counted as motion, so stopping away
  // from the old anchor can become STATIONARY.
  double ref_lat = _state == TrackerMotionState::Moving ? _motion_lat : _anchor_lat;
  double ref_lon = _state == TrackerMotionState::Moving ? _motion_lon : _anchor_lon;
  double dist = distanceM(ref_lat, ref_lon, fix.lat, fix.lon);
  bool disp = dist >= (double)_cfg.move_distance_m;
  bool fast = fix.speed_mps >= _cfg.move_speed_mps;
  if (disp) {
    _speed_run = 0;
  } else if (fast) {
    if (_speed_run < 255) _speed_run++;
  } else {
    _speed_run = 0;
  }
  bool motion = disp || (_speed_run >= _cfg.speed_samples && fast);

  bool send_now = false;
  if (motion) {
    _last_motion_s = now_s;
    _motion_lat = fix.lat;
    _motion_lon = fix.lon;
    if (_state != TrackerMotionState::Moving) {
      _state = TrackerMotionState::Moving;
      send_now = true;
    }
  } else if (_state == TrackerMotionState::Moving) {
    uint32_t held = now_s - _last_motion_s;
    if (held >= _cfg.stationary_hold_s) {
      _state = TrackerMotionState::Stationary;
      _anchor_lat = fix.lat;
      _anchor_lon = fix.lon;
      send_now = true;
    }
  }

  uint16_t interval = _state == TrackerMotionState::Moving ? _cfg.move_interval_s : _cfg.stationary_interval_s;
  if (interval == 0) interval = 1;
  bool due = _have_sent && (now_s - _last_sent_s) >= interval;
  if (send_now || due) {
    out.send = true;
    _have_sent = true;
    _last_sent_s = now_s;
    if (_state == TrackerMotionState::Stationary && !send_now) {
      // Follow slow GPS wander so a parked unit does not walk itself into MOVING.
      _anchor_lat = fix.lat;
      _anchor_lon = fix.lon;
    }
  }

  out.state = _state;
  out.interval_s = interval;
  out.stale_s = staleFor(interval);
  return out;
}
