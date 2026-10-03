# Porting MeshCoreTracker to another GPS radio

This document is an implementation brief. Follow it to add a MeshCore board that has a GPS receiver. Do not re-design the packet, the TAK uid, or the gateway. The LilyGO T-Beam is the worked example and the first radio under test. It is not the interface.

The gateway that consumes these messages is [CopIXus/MeshcoreToTAK](https://github.com/CopIXus/MeshcoreToTAK). A new radio is a firmware port. It is not a protocol change.

## Outcome

After configuration the board boots with no phone attached, reads GPS, and sends `!MT1` on one private MeshCore channel. MeshcoreToTAK plots one stable marker. The advert does not contain the live position.

## Packet contract

MeshCore group text is `<sender name>: <message>`. The sender name is the TAK callsign. Do not put the callsign in the body.

The body is ASCII, semicolon-separated, and starts with the exact prefix `!MT1;`.

```text
!MT1;u=A1B2C3D4;k=k9;la=36.123456;ln=-82.123456;s=1.2;c=90;a=512;st=30;q=1042;b=87
```

| Field | Meaning | Required |
| --- | --- | --- |
| `u` | 8 uppercase hex chars, first 4 bytes of this radio's MeshCore public key | yes |
| `k` | Role key, 2–4 lowercase letters or digits | yes |
| `la` | Latitude, decimal degrees, -90..90 | yes |
| `ln` | Longitude, decimal degrees, -180..180 | yes |
| `s` | Speed, meters per second, 0..200 | no |
| `c` | Course, degrees, 0 inclusive to 360 exclusive | no |
| `a` | Altitude, meters, -1000..20000 | no |
| `st` | TAK stale seconds, 5..3600 | yes |
| `q` | Unsigned 32-bit sequence | yes |
| `b` | Battery percent, 0..100 | no |

Rules the gateway enforces:

- Unknown keys are ignored. Do not reuse `u`, `k`, `la`, `ln`, `s`, `c`, `a`, `st`, `q`, or `b` for something else.
- Duplicate keys are rejected.
- Latitude 0 and longitude 0 together are rejected. Never send a no-fix as `0,0`.
- Color, icon path, and full CoT type are forbidden on the air. They are chosen by MeshcoreToTAK.
- Keep the body at or under 120 characters. MeshCore group text is capped at 160 bytes including the `CALLSIGN: ` prefix (`MAX_TEXT_LEN`). A long callsign eats that budget. Keep the node name near 12 characters.
- Use `trackerFormatFix()` in `tracker/TrackerMessage.cpp`. Do not hand-build a second encoder.

Seed role keys the gateway already maps: `k9`, `veh`, `per`, `fw`, `ems`, `cmd`. Other 2–4 character keys still plot. They use the channel's fallback style until someone adds them on the gateway.

## Identity

- `u` comes from the public key via `trackerIdFromPublicKey()`. It does not change when the operator renames the radio.
- The TAK uid is `meshtracker-` plus `u`. The gateway applies that. The radio does not send the `meshtracker-` prefix.
- Callsign is the MeshCore node name. On the T-Beam tracker build, set it with the USB console `name` command so it is kept across reboot. The MeshCore Android app can also set that name.
- `k` comes from the `role` setting. On the T-Beam tracker build, set it with the USB console `role` command (`k9`, `veh`, `per`, `fw`, `ems`, `cmd`). The MeshCore Android app has no role field. Store the role lowercase. Custom var `trk.role` applies only when a port wires `CMD_SET_CUSTOM_VAR` to it. The T-Beam tracker build does not.
- Group text is not signed. Anyone who has the channel key can claim any name or `u`. The private channel is the trust boundary. Do not put the channel key inside the tracker message.

## Behavior that must stay the same

Use `TrackerMotion` and `trackerDefaults()`. Do not copy the state machine into a board file.

- Advert location sharing is off. Tracking must work with GPS advert share disabled. A one-time local advert may exist for provisioning. It must not be required, and it must not carry the live fix.
- One private channel. Do not open a channel per asset.
- First valid fix sends immediately.
- Moving interval default 15 s. Stationary interval default 300 s. Both are configurable. 5 s is allowed for one or two assets, not the default.
- Enter MOVING on a displacement of at least 20 m immediately, or after 2 consecutive speed samples at or above 1.5 m/s. One noisy speed spike must not flip the state.
- Leave MOVING only after speed stays under the threshold and displacement stays under 20 m for the stationary hold (default 30 s). A stop at a light stays MOVING.
- On the transition into MOVING or STATIONARY, send immediately.
- `st` is `max(interval * 2, stale_min)`. Defaults make 15 s -> 30 s and 300 s -> 600 s.
- `q` increases by one per accepted send. The gateway drops duplicates and older sequences using unsigned 32-bit compare. Initialize `q` from GPS or RTC epoch seconds at boot so a reboot is not treated as a replay of an old low number.
- Application ack (`!MTA1`) stays off unless the operator sets `ack`. An ack roughly doubles airtime. The gateway does not send acks in the first slice.
- Invalid or missing GPS: do not send a position.
- After configuration, boot does not require a phone, BLE connection, or USB host.

Custom variable names, when the Companion `CMD_GET_CUSTOM_VARS` / `CMD_SET_CUSTOM_VAR` path is connected:

```text
trk.en      enabled
trk.ch      channel slot
trk.role    role key k=
trk.mvint   moving interval seconds
trk.stint   stationary interval seconds
trk.mvspd   move speed m/s
trk.mvdst   move distance meters
trk.sthold  stationary hold seconds
trk.stmul   stale multiplier
trk.stmin   stale minimum seconds
trk.ack     application ack
```

Keep the names short. Companion custom-variable frames are small.

## The seam for a new radio

`tracker/` must not include board pins, SX1262 versus SX1276 setup, AXP/PMU code, or a GPS UART driver.

Implement one function that feeds `TrackerMotion::onFix()`:

```text
TrackerFix
  valid       true only for a current fix
  lat, lon    decimal degrees
  speed_mps   ground speed; 0 if the receiver does not provide it
```

Call it on each GPS sample. `now_s` is a monotonic second counter (`millis()/1000` is enough). It is not GPS time.

When `TrackerDecision.send` is true, fill a `TrackerReport` from that fix plus course, altitude, and battery if the board has them, set `stale_sec` from `TrackerDecision.stale_s`, bump `q`, and `trackerFormatFix()` into a buffer. Send that buffer with MeshCore `sendGroupMessage()` on the configured private channel. The send is a flood. Hop limit is the repeater `flood.max` setting, not a field in this message.

GPS time, when the receiver has it, should set the radio clock so the group-text timestamp is close to UTC. If it is more than about 5 minutes from the gateway, MeshcoreToTAK substitutes its own clock and still uses `st`.

## How to add a board

1. Start from that board's current MeshCore Companion variant. In the MeshCore tree that is `variants/<board>/` plus `boards/*.json`. Reuse its radio class, pins, PMU, and the way Companion already powers and reads the GPS. MeshCore is MIT; keep that license on copied firmware.
2. Add a PlatformIO environment named for the board and the radio chip, the same way MeshCore splits `lilygo_tbeam_SX1262` and `lilygo_tbeam_SX1276`.
3. Compile `tracker/*.cpp` into that environment. Do not fork `TrackerMessage` or `TrackerMotion` per board.
4. Wire the variant's GPS sample into the seam above.
5. Set advert location off in the firmware defaults for that environment.
6. Confirm the node name, radio preset, and private channel still come from the normal MeshCore companion app.

The environments `Tbeam_SX1262_meshcore_tracker_ble` and `Tbeam_SX1276_meshcore_tracker_ble` in this repo only compile the core. They are the pattern for the test radios. Copying their `src/main.cpp` onto a new board without the GPS seam does not make a tracker.

Worked example, not a template to paste into `tracker/`:

- SX1262 pins and GPS UART live in MeshCore `variants/lilygo_tbeam_SX1262/` (`PIN_GPS_RX` 12, `PIN_GPS_TX` 34 on that variant).
- SX1276 lives in `variants/lilygo_tbeam_SX1276/`.
- Power and the AXP chip live in `helpers/esp32/TBeamBoard.cpp`.
- Leave those files as the source for T-Beam hardware. Call into them from the board environment. Do not move those pin numbers into `tracker/`.

## Boards to consider later

Do not port these until a T-Beam test sends an `!MT1` that MeshcoreToTAK accepts. They already exist as MeshCore variants with a GPS path:

- Heltec Tracker
- Heltec Wireless Tracker V2
- LilyGO T-Beam Supreme
- LilyGO T-Beam 1W
- Seeed Wio Tracker L1
- SenseCAP T1000-E
- RAK WisMesh Tag

Any other MeshCore variant that already exposes a GPS fix is in scope. A board with no GPS is out of scope.

## Port checklist

- Same private channel key on the radio and on MeshcoreToTAK.
- Tracker parsing enabled on that gateway channel. Chat-to-TAK off if the channel is fixes only.
- Advert has no live GPS.
- One TAK marker uid `meshtracker-` plus `u` across a callsign change.
- `k` selects the gateway role style (`k9`, `veh`, or `per` at minimum).
- A few meters of GPS jitter stays STATIONARY.
- A real move sends immediately, then about every 15 seconds.
- Stopping sends the stationary transition only after the hold.
- The body stays within 120 characters.
- Power cycle without a phone still sends after a fix.
- The gateway's existing advert markers and GeoChat still behave as before. Tracker text does not show up as chat.

## What not to change

On MeshcoreToTAK, do not change the `!MT1` parser, the role catalog lookup order, or `TakCot::buildTrackerPoint()` to suit one board.

Style lookup order, most specific first:

1. Per-tracker override keyed by `u` (callsign override and CoT style).
2. Role catalog entry for `k`.
3. Channel default style, when `k` is missing from the catalog.
4. Gateway default CoT style.

The gateway rejects a bad body and does not post it as chat when tracker parsing is enabled. When tracker parsing is off, the same text stays ordinary chat. Leave that default off in config migration so an update does not turn old channel traffic into positions.

Directed gateway routing and a binary group-data packet are later work. A port does not add them.
