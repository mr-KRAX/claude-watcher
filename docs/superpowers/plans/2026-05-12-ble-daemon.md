# BLE + macOS Menu Bar App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace USB serial with BLE by building a macOS menu bar app (ClaudeWatcher.app) that bridges Claude Code hook signals (Unix socket) to the ESP32 display over BLE.

**Architecture:** Claude Code hook scripts write ASCII state messages to a Unix socket. A rumps-based macOS app receives them, updates its menu bar, and forwards them to the ESP32 via bleak BLE. The ESP32 runs a NimBLE peripheral; serial port remains open for debug logs only.

**Tech Stack:** Python 3.9+, rumps, bleak, py2app, NimBLE-Arduino (ESP32), Arduino IDE

---

## File Map

### New files
- `macos/ClaudeWatcher/__init__.py` — empty package marker
- `macos/ClaudeWatcher/config.py` — UUIDs, socket path, retry interval
- `macos/ClaudeWatcher/ipc_server.py` — Unix socket server (background thread)
- `macos/ClaudeWatcher/ble_manager.py` — BLE client thread (bleak + asyncio)
- `macos/ClaudeWatcher/app.py` — rumps App; owns menu bar, queue, thread lifecycle
- `macos/main.py` — py2app entry point
- `macos/setup.py` — py2app build config
- `macos/tests/test_ipc_server.py` — IPC server unit tests
- `macos/tests/test_ble_manager.py` — BLE manager unit tests
- `macos/ipc_send.py` — replaces serial_send.py
- `esp32/claude_watcher/ble_handler.h` — BLE peripheral interface
- `esp32/claude_watcher/ble_handler.cpp` — NimBLE peripheral implementation

### Modified files
- `macos/on_pre_tool.py` — import ipc_send instead of serial_send
- `macos/on_stop.py` — import ipc_send instead of serial_send
- `macos/on_notification.py` — import ipc_send instead of serial_send
- `macos/requirements.txt` — add rumps, bleak, py2app; remove pyserial
- `macos/install.sh` — rebuild: build .app, copy to /Applications, install launchd plist
- `esp32/claude_watcher/claude_watcher.ino` — remove serial read loop, add BLE init

### Unchanged files
- `esp32/claude_watcher/serial_handler.h/.cpp` — kept; provides `State` enum and `parseMessage()`
- `esp32/claude_watcher/display.h/.cpp` — unchanged
- `esp32/claude_watcher/crab_sprites.h` — unchanged

---

## Task 1: config.py — constants

**Files:**
- Create: `macos/ClaudeWatcher/__init__.py`
- Create: `macos/ClaudeWatcher/config.py`

- [ ] **Step 1: Create the package marker**

```bash
mkdir -p macos/ClaudeWatcher
touch macos/ClaudeWatcher/__init__.py
```

- [ ] **Step 2: Write config.py**

```python
# macos/ClaudeWatcher/config.py
SERVICE_UUID       = "12345678-1234-1234-1234-123456789abc"
CHAR_UUID          = "12345678-1234-1234-1234-123456789abd"
SOCKET_PATH        = "/tmp/claude-watcher.sock"
BLE_RETRY_INTERVAL = 5  # seconds between reconnect attempts
```

- [ ] **Step 3: Commit**

```bash
git add macos/ClaudeWatcher/__init__.py macos/ClaudeWatcher/config.py
git commit -m "feat: add ClaudeWatcher package with config constants"
```

---

## Task 2: ipc_server.py — Unix socket server

**Files:**
- Create: `macos/ClaudeWatcher/ipc_server.py`
- Create: `macos/tests/test_ipc_server.py`

- [ ] **Step 1: Write the failing tests**

```python
# macos/tests/test_ipc_server.py
import os
import socket
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from ClaudeWatcher.ipc_server import IPCServer


def test_receives_message(tmp_path):
    sock_path = str(tmp_path / "test.sock")
    received = []

    server = IPCServer(received.append, socket_path=sock_path)
    server.start()
    time.sleep(0.05)

    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
        s.connect(sock_path)
        s.sendall(b"WORKING:Bash")

    time.sleep(0.05)
    assert received == ["WORKING:Bash"]


def test_strips_whitespace(tmp_path):
    sock_path = str(tmp_path / "ws.sock")
    received = []

    server = IPCServer(received.append, socket_path=sock_path)
    server.start()
    time.sleep(0.05)

    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
        s.connect(sock_path)
        s.sendall(b"  WAITING  ")

    time.sleep(0.05)
    assert received == ["WAITING"]


def test_ignores_empty_message(tmp_path):
    sock_path = str(tmp_path / "empty.sock")
    received = []

    server = IPCServer(received.append, socket_path=sock_path)
    server.start()
    time.sleep(0.05)

    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
        s.connect(sock_path)
        s.sendall(b"   ")

    time.sleep(0.05)
    assert received == []


def test_cleans_up_stale_socket(tmp_path):
    sock_path = str(tmp_path / "stale.sock")
    open(sock_path, "w").close()  # create stale file

    server = IPCServer(lambda _: None, socket_path=sock_path)
    server.start()  # must not raise
    time.sleep(0.05)
    assert os.path.exists(sock_path)  # new socket exists


def test_multiple_sequential_messages(tmp_path):
    sock_path = str(tmp_path / "multi.sock")
    received = []

    server = IPCServer(received.append, socket_path=sock_path)
    server.start()
    time.sleep(0.05)

    for msg in [b"WORKING:Bash", b"WAITING", b"IDLE"]:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(sock_path)
            s.sendall(msg)
        time.sleep(0.03)

    assert received == ["WORKING:Bash", "WAITING", "IDLE"]
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cd macos && python -m pytest tests/test_ipc_server.py -v
```

Expected: `ImportError: cannot import name 'IPCServer'`

- [ ] **Step 3: Implement ipc_server.py**

```python
# macos/ClaudeWatcher/ipc_server.py
import os
import socket
import threading

from ClaudeWatcher.config import SOCKET_PATH


class IPCServer:
    def __init__(self, on_message, socket_path=None):
        self._on_message = on_message
        self._socket_path = socket_path or SOCKET_PATH

    def start(self):
        if os.path.exists(self._socket_path):
            os.unlink(self._socket_path)
        t = threading.Thread(target=self._serve, daemon=True)
        t.start()

    def _serve(self):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as srv:
            srv.bind(self._socket_path)
            srv.listen(5)
            while True:
                conn, _ = srv.accept()
                threading.Thread(target=self._handle, args=(conn,), daemon=True).start()

    def _handle(self, conn):
        with conn:
            data = conn.recv(256)
            if data:
                msg = data.decode("utf-8", errors="ignore").strip()
                if msg:
                    self._on_message(msg)
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
cd macos && python -m pytest tests/test_ipc_server.py -v
```

Expected: 5 tests PASS

- [ ] **Step 5: Commit**

```bash
git add macos/ClaudeWatcher/ipc_server.py macos/tests/test_ipc_server.py
git commit -m "feat: add IPCServer (Unix socket, background thread)"
```

---

## Task 3: ble_manager.py — BLE client

**Files:**
- Create: `macos/ClaudeWatcher/ble_manager.py`
- Create: `macos/tests/test_ble_manager.py`

- [ ] **Step 1: Install bleak in the venv if not present**

```bash
cd macos && source venv/bin/activate && pip install bleak
```

- [ ] **Step 2: Write the failing tests**

```python
# macos/tests/test_ble_manager.py
import asyncio
import os
import queue
import sys
import time
from unittest.mock import AsyncMock, MagicMock, patch

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from ClaudeWatcher.ble_manager import BLEManager


def test_scanning_status_emitted_when_no_device():
    """Status callback receives 'Scanning…' when no BLE device is found."""
    statuses = []
    q = queue.Queue(maxsize=1)

    async def mock_find(*args, **kwargs):
        return None

    with patch("ClaudeWatcher.ble_manager.BleakScanner") as MockScanner:
        MockScanner.find_device_by_filter = AsyncMock(return_value=None)
        mgr = BLEManager(q, statuses.append, retry_interval=0.05)
        mgr.start()
        time.sleep(0.3)

    assert "Scanning…" in statuses


def test_write_called_with_queued_message():
    """BLEManager writes queued message bytes to the BLE characteristic."""
    written = []
    statuses = []
    q = queue.Queue(maxsize=1)
    q.put("WORKING:Bash")

    mock_device = MagicMock()
    mock_client = AsyncMock()
    mock_client.is_connected = True

    async def mock_write(char_uuid, data, response=False):
        written.append(data)
        mock_client.is_connected = False  # stop after first write

    mock_client.connect = AsyncMock()
    mock_client.disconnect = AsyncMock()
    mock_client.write_gatt_char = mock_write

    with patch("ClaudeWatcher.ble_manager.BleakScanner") as MockScanner, \
         patch("ClaudeWatcher.ble_manager.BleakClient", return_value=mock_client):
        MockScanner.find_device_by_filter = AsyncMock(return_value=mock_device)
        mgr = BLEManager(q, statuses.append, retry_interval=0.05)
        mgr.start()
        time.sleep(0.5)

    assert b"WORKING:Bash" in written


def test_connected_status_emitted_on_connect():
    """Status callback receives 'Connected' after a successful BLE connection."""
    statuses = []
    q = queue.Queue(maxsize=1)

    mock_device = MagicMock()
    mock_client = AsyncMock()
    mock_client.is_connected = False  # immediately disconnect to end send loop

    mock_client.connect = AsyncMock()
    mock_client.disconnect = AsyncMock()
    mock_client.write_gatt_char = AsyncMock()

    with patch("ClaudeWatcher.ble_manager.BleakScanner") as MockScanner, \
         patch("ClaudeWatcher.ble_manager.BleakClient", return_value=mock_client):
        MockScanner.find_device_by_filter = AsyncMock(return_value=mock_device)
        mgr = BLEManager(q, statuses.append, retry_interval=0.5)
        mgr.start()
        time.sleep(0.3)

    assert "Connected" in statuses
```

- [ ] **Step 3: Run tests to verify they fail**

```bash
cd macos && python -m pytest tests/test_ble_manager.py -v
```

Expected: `ImportError: cannot import name 'BLEManager'`

- [ ] **Step 4: Implement ble_manager.py**

```python
# macos/ClaudeWatcher/ble_manager.py
import asyncio
import queue
import threading

from bleak import BleakClient, BleakScanner

from ClaudeWatcher.config import BLE_RETRY_INTERVAL, CHAR_UUID, SERVICE_UUID


class BLEManager:
    def __init__(self, msg_queue: queue.Queue, on_status_change, retry_interval=None):
        self._queue = msg_queue
        self._on_status = on_status_change
        self._retry = retry_interval if retry_interval is not None else BLE_RETRY_INTERVAL

    def start(self):
        t = threading.Thread(target=self._run, daemon=True)
        t.start()

    def _run(self):
        asyncio.run(self._ble_loop())

    async def _ble_loop(self):
        while True:
            self._on_status("Scanning…")
            device = await BleakScanner.find_device_by_filter(
                lambda d, _: SERVICE_UUID.lower() in [
                    str(s).lower() for s in (d.metadata.get("uuids") or [])
                ],
                timeout=5.0,
            )
            if device is not None:
                client = BleakClient(device)
                try:
                    await client.connect()
                    self._on_status("Connected")
                    await self._send_loop(client)
                except Exception:
                    pass
                finally:
                    self._on_status("Reconnecting…")
                    try:
                        await client.disconnect()
                    except Exception:
                        pass
            await asyncio.sleep(self._retry)

    async def _send_loop(self, client: BleakClient):
        while client.is_connected:
            try:
                msg = self._queue.get_nowait()
                await client.write_gatt_char(CHAR_UUID, msg.encode("utf-8"), response=False)
            except queue.Empty:
                await asyncio.sleep(0.05)
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
cd macos && python -m pytest tests/test_ble_manager.py -v
```

Expected: 3 tests PASS

- [ ] **Step 6: Commit**

```bash
git add macos/ClaudeWatcher/ble_manager.py macos/tests/test_ble_manager.py
git commit -m "feat: add BLEManager (bleak BLE client, background asyncio thread)"
```

---

## Task 4: app.py + main.py — rumps menu bar app

**Files:**
- Create: `macos/ClaudeWatcher/app.py`
- Create: `macos/main.py`

Note: `rumps` requires a macOS AppKit run loop — no automated unit tests. Verify manually after building.

- [ ] **Step 1: Write app.py**

```python
# macos/ClaudeWatcher/app.py
import queue

import rumps

from ClaudeWatcher.ble_manager import BLEManager
from ClaudeWatcher.ipc_server import IPCServer

_STATE_ICONS = {
    "WORKING": "🦀",
    "WAITING": "🦀",
    "IDLE":    "💤",
}


class ClaudeWatcherApp(rumps.App):
    def __init__(self):
        super().__init__("💤")
        self._msg_queue = queue.Queue(maxsize=1)

        # Shared state (written from background threads, read from main thread timer)
        self._state_label = "IDLE"
        self._tool_label  = ""
        self._ble_status  = "—"

        self._state_item = rumps.MenuItem("State: IDLE")
        self._ble_item   = rumps.MenuItem("BLE: —")
        self.menu = [self._state_item, self._ble_item, None, "Quit"]

        IPCServer(self._on_message).start()
        BLEManager(self._msg_queue, self._on_ble_status).start()

        rumps.Timer(self._refresh_ui, 0.2).start()

    # ── Background-thread callbacks (write plain vars only) ──────────────────

    def _on_message(self, msg: str):
        if msg.startswith("WORKING:"):
            self._state_label = "WORKING"
            self._tool_label  = msg[8:]
        elif msg in ("WAITING", "WAITING_URGENT"):
            self._state_label = "WAITING"
            self._tool_label  = ""
        elif msg == "IDLE":
            self._state_label = "IDLE"
            self._tool_label  = ""

        # Keep only the latest message in the queue
        try:
            self._msg_queue.get_nowait()
        except queue.Empty:
            pass
        self._msg_queue.put_nowait(msg)

    def _on_ble_status(self, status: str):
        self._ble_status = status

    # ── Main-thread timer: update rumps UI ───────────────────────────────────

    def _refresh_ui(self, _):
        icon = _STATE_ICONS.get(self._state_label, "🦀")
        self.title = icon
        label = f"State: {self._state_label}"
        if self._tool_label:
            label += f" ({self._tool_label})"
        self._state_item.title = label
        self._ble_item.title   = f"BLE: {self._ble_status}"


def main():
    ClaudeWatcherApp().run()
```

- [ ] **Step 2: Write main.py**

```python
# macos/main.py
from ClaudeWatcher.app import main

if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Smoke-test the app directly (no BLE device needed)**

```bash
cd macos && source venv/bin/activate && pip install rumps && python main.py
```

Expected: a 💤 icon appears in the macOS menu bar. Click it — menu shows `State: IDLE` and `BLE: —`. No crash.

Send a test message via a second terminal:

```bash
python3 -c "
import socket
with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
    s.connect('/tmp/claude-watcher.sock')
    s.sendall(b'WORKING:Bash')
"
```

Expected: icon stays 🦀, menu updates to `State: WORKING (Bash)`.

Kill the app with Ctrl-C.

- [ ] **Step 4: Commit**

```bash
git add macos/ClaudeWatcher/app.py macos/main.py
git commit -m "feat: add rumps menu bar app (ClaudeWatcherApp)"
```

---

## Task 5: ipc_send.py + update hook scripts

**Files:**
- Create: `macos/ipc_send.py`
- Modify: `macos/on_pre_tool.py`
- Modify: `macos/on_stop.py`
- Modify: `macos/on_notification.py`

- [ ] **Step 1: Write ipc_send.py**

```python
# macos/ipc_send.py
import socket
import sys

SOCK_PATH = "/tmp/claude-watcher.sock"


def send(message: str):
    """Send an ASCII message to ClaudeWatcher.app via Unix socket. Silent fail."""
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(SOCK_PATH)
            s.sendall(message.encode("utf-8"))
    except Exception as e:
        print(f"claude-watcher: {e}", file=sys.stderr)
```

- [ ] **Step 2: Update on_pre_tool.py**

Replace:
```python
import serial_send
...
serial_send.send(f"WORKING:{tool_name}")
```
With:
```python
#!/usr/bin/env python3
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ipc_send


def main():
    payload = json.loads(sys.stdin.read())
    tool_name = payload.get("tool_name", "Unknown")
    ipc_send.send(f"WORKING:{tool_name}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Update on_stop.py**

```python
#!/usr/bin/env python3
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ipc_send


def main():
    sys.stdin.read()  # consume stdin
    ipc_send.send("WAITING")


if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Update on_notification.py**

```python
#!/usr/bin/env python3
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ipc_send


def main():
    sys.stdin.read()  # consume stdin
    ipc_send.send("WAITING_URGENT")


if __name__ == "__main__":
    main()
```

- [ ] **Step 5: Verify ipc_send works independently**

Start the app (`python main.py`) in one terminal, then:

```bash
cd macos && python -c "import ipc_send; ipc_send.send('WAITING')"
```

Expected: menu bar updates to `State: WAITING`. No error output.

- [ ] **Step 6: Commit**

```bash
git add macos/ipc_send.py macos/on_pre_tool.py macos/on_stop.py macos/on_notification.py
git commit -m "feat: replace serial_send with ipc_send (Unix socket)"
```

---

## Task 6: requirements.txt, setup.py, install.sh

**Files:**
- Modify: `macos/requirements.txt`
- Create: `macos/setup.py`
- Modify: `macos/install.sh`

- [ ] **Step 1: Update requirements.txt**

```
rumps>=0.4.0
bleak>=0.21.0
py2app>=0.28.0
pytest>=7.0
```

- [ ] **Step 2: Write setup.py**

```python
# macos/setup.py
from setuptools import setup

APP = ["main.py"]
OPTIONS = {
    "argv_emulation": False,
    "plist": {
        "LSUIElement": True,
        "CFBundleName": "ClaudeWatcher",
        "CFBundleDisplayName": "Claude Watcher",
        "CFBundleIdentifier": "com.kraalex.claude-watcher",
        "CFBundleVersion": "1.0.0",
        "NSBluetoothAlwaysUsageDescription": "Sends status to ESP32 display over BLE.",
    },
    "packages": ["ClaudeWatcher", "bleak", "rumps"],
}

setup(
    app=APP,
    options={"py2app": OPTIONS},
    setup_requires=["py2app"],
)
```

- [ ] **Step 3: Write install.sh**

```bash
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "==> Installing dependencies..."
python3 -m venv .venv
source .venv/bin/activate
pip install -q -r requirements.txt

echo "==> Building ClaudeWatcher.app..."
rm -rf build dist
python setup.py py2app 2>&1 | tail -10

APP_SRC="$SCRIPT_DIR/dist/ClaudeWatcher.app"
APP_DST="/Applications/ClaudeWatcher.app"

echo "==> Installing to /Applications..."
rm -rf "$APP_DST"
cp -r "$APP_SRC" "$APP_DST"

PLIST_DIR="$HOME/Library/LaunchAgents"
PLIST="$PLIST_DIR/com.kraalex.claude-watcher.plist"
mkdir -p "$PLIST_DIR"

cat > "$PLIST" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.kraalex.claude-watcher</string>
    <key>ProgramArguments</key>
    <array>
        <string>/Applications/ClaudeWatcher.app/Contents/MacOS/ClaudeWatcher</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
</dict>
</plist>
EOF

launchctl unload "$PLIST" 2>/dev/null || true
launchctl load "$PLIST"
echo "==> Done. ClaudeWatcher is running and will start on login."
```

- [ ] **Step 4: Make install.sh executable**

```bash
chmod +x macos/install.sh
```

- [ ] **Step 5: Test the build**

```bash
cd macos && ./install.sh
```

Expected: `ClaudeWatcher.app` appears in `/Applications`, 💤 icon in menu bar.

- [ ] **Step 6: Commit**

```bash
git add macos/requirements.txt macos/setup.py macos/install.sh
git commit -m "feat: add py2app build and launchd install script"
```

---

## Task 7: ESP32 — ble_handler.h/.cpp (NimBLE peripheral)

**Files:**
- Create: `esp32/claude_watcher/ble_handler.h`
- Create: `esp32/claude_watcher/ble_handler.cpp`

Prerequisite: install `NimBLE-Arduino` in Arduino IDE via Library Manager (search "NimBLE-Arduino" by h2zero).

- [ ] **Step 1: Write ble_handler.h**

```cpp
// esp32/claude_watcher/ble_handler.h
#pragma once
#include <Arduino.h>
#include "serial_handler.h"   // provides State enum and parseMessage()

// Callback fired on each valid BLE write.
// state:    parsed state
// toolName: tool name (non-empty only for WORKING)
typedef void (*BLEStateCallback)(State state, const char* toolName);

// Initialize NimBLE peripheral, start advertising.
// callback is invoked from the NimBLE task on each valid characteristic write.
void bleInit(BLEStateCallback callback);
```

- [ ] **Step 2: Write ble_handler.cpp**

```cpp
// esp32/claude_watcher/ble_handler.cpp
#include "ble_handler.h"
#include <NimBLEDevice.h>

#define SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define CHAR_UUID    "12345678-1234-1234-1234-123456789abd"

static BLEStateCallback g_callback = nullptr;

class CharCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        std::string raw = pChar->getValue();
        String val(raw.c_str());
        Serial.println("[BLE] " + val);

        if (!g_callback) return;

        State newState = State::IDLE;
        char toolName[32] = "";
        if (parseMessage(val, &newState, toolName, sizeof(toolName))) {
            g_callback(newState, toolName);
        }
    }
};

void bleInit(BLEStateCallback callback) {
    g_callback = callback;

    NimBLEDevice::init("ClaudeWatcher");

    NimBLEServer* pServer = NimBLEDevice::createServer();

    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    NimBLECharacteristic* pChar = pService->createCharacteristic(
        CHAR_UUID,
        NIMBLE_PROPERTY::WRITE_NR   // write without response
    );
    pChar->setCallbacks(new CharCallbacks());

    pService->start();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->start();

    Serial.println("[BLE] Advertising as ClaudeWatcher");
}
```

- [ ] **Step 3: Verify it compiles**

In Arduino IDE: open `esp32/claude_watcher/claude_watcher.ino` → Sketch → Verify/Compile.

Expected: compile error about missing `bleInit` call (that's fine — we haven't updated the .ino yet). The goal here is that `ble_handler.cpp` itself compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add esp32/claude_watcher/ble_handler.h esp32/claude_watcher/ble_handler.cpp
git commit -m "feat: add NimBLE peripheral handler for ESP32"
```

---

## Task 8: ESP32 — update claude_watcher.ino

**Files:**
- Modify: `esp32/claude_watcher/claude_watcher.ino`

- [ ] **Step 1: Replace the .ino contents**

Open `esp32/claude_watcher/claude_watcher.ino` in Arduino IDE and replace with:

```cpp
#include <TFT_eSPI.h>
#include "ble_handler.h"
#include "display.h"

#define SERIAL_BAUD     115200
#define IDLE_TIMEOUT_MS (5UL * 60UL * 1000UL)

// ── Button (IO14, active-low) ──────────────────────────────────────────────
#define BTN_PIN          14
#define DOUBLE_CLICK_MS  400
#define DEBOUNCE_MS       30

TFT_eSPI tft = TFT_eSPI();

State    currentState    = State::IDLE;
char     toolName[32]    = "";
uint32_t lastMsgMs       = 0;
uint8_t  currentRotation = 0;

// Button state
static bool     btnLast     = HIGH;
static uint8_t  clickCount  = 0;
static uint32_t lastClickMs = 0;

// BLE callback — runs on NimBLE FreeRTOS task
static void onBLEState(State state, const char* tool) {
    currentState = state;
    strncpy(toolName, tool, sizeof(toolName) - 1);
    toolName[sizeof(toolName) - 1] = '\0';
    lastMsgMs = millis();
}

static bool pollButton(uint32_t now) {
    bool btnNow = digitalRead(BTN_PIN);
    if (btnLast == HIGH && btnNow == LOW) {
        if (now - lastClickMs > DEBOUNCE_MS) {
            clickCount++;
            lastClickMs = now;
        }
    }
    btnLast = btnNow;
    if (clickCount > 0 && (now - lastClickMs) > DOUBLE_CLICK_MS) {
        uint8_t n = clickCount;
        clickCount = 0;
        return n >= 2;
    }
    return false;
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    pinMode(BTN_PIN, INPUT_PULLUP);
    displayInit(tft);
    bleInit(onBLEState);
    lastMsgMs = millis();
}

void loop() {
    uint32_t now = millis();

    if (pollButton(now)) {
        currentRotation = (currentRotation + 1) % 4;
        displaySetRotation(tft, currentRotation, currentState, toolName);
    }

    if (currentState != State::IDLE && (now - lastMsgMs >= IDLE_TIMEOUT_MS)) {
        currentState = State::IDLE;
        toolName[0] = '\0';
    }

    displayUpdate(tft, currentState, toolName, now);
}
```

- [ ] **Step 2: Compile in Arduino IDE**

Sketch → Verify/Compile.

Expected: clean compile. If `NimBLE-Arduino` is missing, install it via Tools → Manage Libraries.

- [ ] **Step 3: Flash and verify**

Upload to the board. Open Serial Monitor at 115200 baud.

Expected output on boot:
```
[BLE] Advertising as ClaudeWatcher
```

On macOS, open Bluetooth settings or run:
```bash
python3 -c "
import asyncio
from bleak import BleakScanner
async def scan():
    devices = await BleakScanner.discover(timeout=5)
    for d in devices:
        print(d.name, d.address)
asyncio.run(scan())
"
```

Expected: `ClaudeWatcher` appears in the device list.

- [ ] **Step 4: Commit**

```bash
git add esp32/claude_watcher/claude_watcher.ino
git commit -m "feat: replace serial input with BLE peripheral in main sketch"
```

---

## Task 9: End-to-end integration test

No new files. Manual verification only.

- [ ] **Step 1: Start the app**

```bash
/Applications/ClaudeWatcher.app/Contents/MacOS/ClaudeWatcher
```

Or let launchd start it automatically. Confirm 💤 in menu bar.

- [ ] **Step 2: Wait for BLE connection**

Expected: within ~10s, menu shows `BLE: Connected`. Serial Monitor on the ESP32 shows a BLE connection message.

- [ ] **Step 3: Simulate a hook message**

```bash
python3 -c "
import socket
with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
    s.connect('/tmp/claude-watcher.sock')
    s.sendall(b'WORKING:Bash')
"
```

Expected:
- Menu bar: 🦀, `State: WORKING (Bash)`, `BLE: Connected`
- ESP32 display: WORKING animation, status bar shows `⣾ Bash`
- Serial Monitor: `[BLE] WORKING:Bash`

- [ ] **Step 4: Test WAITING**

```bash
python3 -c "
import socket
with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
    s.connect('/tmp/claude-watcher.sock')
    s.sendall(b'WAITING')
"
```

Expected: ESP32 switches to WAITING animation; menu shows `State: WAITING`.

- [ ] **Step 5: Run the full pytest suite**

```bash
cd macos && python -m pytest tests/ -v
```

Expected: all tests PASS (ipc_server + ble_manager).

- [ ] **Step 6: Final commit**

```bash
git add -A
git commit -m "feat: complete BLE + macOS menu bar app integration"
```
