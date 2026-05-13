# ESP32 Updates — Design Spec
_2026-05-13_

## Overview

Four improvements to the ESP32 firmware:
1. New `NOTIFICATION` overlay event with blinking blue border
2. IDLE zzz animation reworked as a sequential snake
3. WAITING `!` marks repositioned for horizontal orientation
4. BLE reconnect fix — board re-advertises after disconnect

---

## 1. NOTIFICATION Event

### Protocol

New BLE message: `NOTIFICATION\n`

Parsed in `serial_handler` as a separate `Event` (not part of `State` enum) so the crab state machine is untouched.

### `.ino` logic

- Add `uint32_t notifStartMs = 0` (0 = inactive)
- On `NOTIFICATION` message: `notifStartMs = now`
- On any other BLE message (WORKING, WAITING, IDLE): `notifStartMs = 0`
- Compute `notifActive = (notifStartMs != 0 && now - notifStartMs < 120000UL)`
- Pass `notifActive` as a new `bool` parameter to `displayUpdate`

### Display rendering

- When `notifActive`: draw a 4px blue rectangle border along the screen edges
- Blink: `bool frameOn = (now / 400) % 2` — toggles every 400ms, no extra timer state
- Track `static bool lastNotifFrame` to avoid unnecessary redraws
- When notification deactivates: erase the border (draw bg-colored rectangle over edges)

---

## 2. IDLE ZZZ Snake Animation

### Current behavior

3 z's move upward in lockstep — all driven by the same `zOffset` counter.

### New behavior

Single `zPos` counter increments each tick. Each z's screen Y:

```
y_i = baseY - (zPos - i * ZZZ_STEP)
```

Only drawn when Y is within the visible window above the crab. This staggers the z's so they flow sequentially — z3 starts just as z1 reaches the top and vanishes.

`zPos` wraps at `ZZZ_COUNT * ZZZ_STEP * 2` to allow each z to complete its full travel before the next cycle.

### Orientation

- **Portrait (rotations 0 & 2):** Snake on right side of crab only
- **Horizontal (rotations 1 & 3):** Snake mirrored on both sides simultaneously
  - Right: between crab right edge and screen right edge
  - Left: symmetric position between crab left edge and screen left edge
  - Same `zPos` drives both sides

---

## 3. WAITING `!` in Horizontal Orientation

### Current behavior

Single `!` blinks above the crab head in all orientations.

### New behavior

- **Portrait (rotations 0 & 2):** unchanged — single `!` above the head
- **Horizontal (rotations 1 & 3):** two `!` marks blink simultaneously
  - Left `!`: between crab left edge and screen left edge (same x zone as left zzz in idle)
  - Right `!`: between crab right edge and screen right edge (same x zone as right zzz in idle)
  - Same `bangVisible` toggle, same 500ms blink rate, same amber color

`drawBang()` gains an `isHorizontal` bool and draws one or two marks accordingly.

---

## 4. BLE Reconnect Fix

### Root cause

NimBLE stops advertising once a client connects. Without a disconnect callback, advertising is never restarted — the board becomes invisible after the macOS app disconnects, requiring a board reboot.

### Fix

Add `NimBLEServerCallbacks` to `ble_handler.cpp`:

```cpp
class ServerCallbacks : public NimBLEServerCallbacks {
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        NimBLEDevice::getAdvertising()->start();
    }
};
```

Set via `pServer->setCallbacks(new ServerCallbacks())` in `bleInit()`. The board re-advertises immediately after any disconnection.

---

## Files Changed

| File | Change |
|---|---|
| `serial_handler.h` | Add `Event` enum with `NOTIFICATION`; `parseMessage` returns `Event` or distinguishes notification |
| `serial_handler.cpp` | Parse `NOTIFICATION` message |
| `ble_handler.cpp` | Add `ServerCallbacks::onDisconnect` → restart advertising |
| `display.h` | `displayUpdate` gains `bool notifActive` param |
| `display.cpp` | Notification border overlay; snake zzz; horizontal `!` marks |
| `claude_watcher.ino` | Track `notifStartMs`; pass `notifActive` to `displayUpdate`; clear notif on any state message |

---

## Out of Scope

- Notification sound or haptic feedback
- Per-notification content (text, source)
- Notification queue (only one active at a time)
