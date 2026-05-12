# Claude Watcher — BLE + macOS App Design Spec
_2026-05-12_

## Overview

Replaces the USB serial communication channel with BLE. A native macOS menu bar app (`ClaudeWatcher.app`) acts as the bridge: it receives Claude Code hook signals over a Unix socket, displays current state in the menu bar, and forwards state changes to the ESP32 over BLE. The ESP32 serial port is retained for debug logging only.

---

## Architecture

```
Claude Code (macOS)
  └── hooks (PreToolUse / Stop / Notification)
       └── on_*.py  ──[Unix socket /tmp/claude-watcher.sock]──► ClaudeWatcher.app
                                                                    ├── rumps menu bar
                                                                    │    ├── 🦀 icon (state-colored)
                                                                    │    ├── State: WORKING / WAITING / IDLE
                                                                    │    ├── BLE: Connected / Scanning / —
                                                                    │    └── Quit
                                                                    └── BLE (bleak)
                                                                         └──[BLE]──► ESP32
                                                                                      ├── BLE peripheral
                                                                                      ├── display (unchanged)
                                                                                      └── Serial → debug logs only
```

Three concurrent pieces inside the app process:
1. **rumps run loop** — owns the menu bar, runs on the main thread
2. **Unix socket server** — background thread; receives messages from hook scripts, puts them on a queue
3. **BLE manager** — background thread with its own asyncio loop; drains the queue and writes to the ESP32 BLE characteristic

---

## BLE Protocol

The ESP32 is a **BLE peripheral** (server). The Mac app is the **central** (client).

### UUIDs

```
Service:        12345678-1234-1234-1234-123456789abc
Characteristic: 12345678-1234-1234-1234-123456789abd  (WRITE, no response)
```

### Messages

Plain ASCII, no newline (BLE write framing replaces newline termination):

| Payload | Meaning |
|---|---|
| `WORKING:<tool>` | Claude executing a tool (e.g. `WORKING:Bash`) |
| `WAITING` | Claude finished its turn, waiting for user reply |
| `WAITING_URGENT` | Claude needs permission (Notification hook) |
| `IDLE` | Explicit idle signal |

### Connection Handling

- On launch, app scans for a device advertising the service UUID
- Stays connected after first message (no disconnect per message)
- On disconnect: menu shows `BLE: Reconnecting…`, retries every 5s
- Messages received while disconnected: queue kept, only the latest state matters (queue depth 1)

---

## macOS App (`macos/ClaudeWatcher/`)

### Menu Bar

```
🦀  ───────────────
    State:   WORKING (Bash)
    BLE:     Connected
    ───────────────
    Quit
```

Icon per state:
| State | Icon | Color |
|---|---|---|
| WORKING | 🦀 | orange |
| WAITING | 🦀 | amber |
| IDLE | 💤 | grey |

### IPC — Unix Socket Server (`ipc_server.py`)

- Listens on `/tmp/claude-watcher.sock`
- Background thread, started on app launch
- Accepts short ASCII messages from hook scripts
- Hook scripts connect → write → disconnect (fire-and-forget, no response)
- Valid messages are placed on a thread-safe queue consumed by the BLE manager

### BLE Manager (`ble_manager.py`)

- Runs in a background thread with a dedicated asyncio event loop
- Scans by service UUID, connects to first matching device
- On new queue item: writes payload to the characteristic (WRITE_WITHOUT_RESPONSE)
- Reconnect loop: 5s retry on disconnect

### Components

| File | Responsibility |
|---|---|
| `app.py` | rumps App subclass, menu items, state/BLE status updates |
| `ipc_server.py` | Unix socket server thread |
| `ble_manager.py` | bleak BLE thread + asyncio loop |
| `config.py` | UUIDs, socket path, constants |

### File Tree

```
macos/
├── ClaudeWatcher/
│   ├── app.py
│   ├── ipc_server.py
│   ├── ble_manager.py
│   └── config.py
├── setup.py            # py2app config
├── requirements.txt    # rumps, bleak
└── install.sh          # builds .app, copies to /Applications, installs launchd plist
```

### Installation

```bash
# Prerequisites
pip install rumps bleak py2app

# Build and install
./macos/install.sh
```

`install.sh` will:
1. Build the `.app` with `python setup.py py2app`
2. Copy `ClaudeWatcher.app` to `/Applications`
3. Install a launchd user agent plist to `~/Library/LaunchAgents/` and load it

---

## ESP32 Changes (`esp32/claude_watcher/`)

### What Changes

- **Removed:** `serial_handler.cpp/h` (serial line parsing)
- **Added:** `ble_handler.cpp/h` — BLE peripheral using `NimBLE-Arduino` library
- **Unchanged:** `display.cpp/h`, `crab_sprites.h`, state machine, idle timeout, button rotation

### BLE Setup

```cpp
// setup():
Serial.begin(115200);          // debug only
BLE peripheral init;
Advertise service UUID;

// On characteristic write:
void onWrite(BLECharacteristic* c) {
    String val = c->getValue();
    State newState = currentState;
    if (parseMessage(val, &newState, toolName, sizeof(toolName))) {
        Serial.println("[BLE] " + val);   // debug log
        currentState = newState;
        lastMsgMs = millis();
    }
}
```

`parseMessage()` is reused from the existing `serial_handler.cpp` without modification.

### File Tree

```
esp32/claude_watcher/
├── claude_watcher.ino       # setup(), loop(), BLE init, state dispatch
├── ble_handler.h/.cpp       # BLE peripheral, characteristic callback
├── display.h/.cpp           # unchanged
├── crab_sprites.h           # unchanged
└── serial_handler.h/.cpp    # kept as parseMessage() utility, serial input removed
```

---

## Hook Scripts

Unchanged in logic. Only `serial_send.py` is replaced by `ipc_send.py`:

```python
# ipc_send.py — replaces serial_send.py
import socket, sys

SOCK_PATH = "/tmp/claude-watcher.sock"

def send(message: str):
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(SOCK_PATH)
            s.sendall(message.encode())
    except Exception as e:
        print(f"[claude-watcher] {e}", file=sys.stderr)
```

---

## Dependencies

### macOS App
- Python 3.9+
- `rumps` — menu bar framework
- `bleak` — cross-platform BLE (uses CoreBluetooth on macOS)
- `py2app` — bundle as `.app`

### ESP32
- Arduino IDE with ESP32-S3 board support
- `NimBLE-Arduino` library (lighter BLE stack than default ESP32 BLE)
- `TFT_eSPI` (unchanged)

---

## Out of Scope

- Multiple simultaneous Claude sessions
- BLE pairing/bonding (open connection, no auth)
- Brightness control or power saving on the ESP32
- WiFi / cloud communication
- Manual connect/disconnect controls in the menu
