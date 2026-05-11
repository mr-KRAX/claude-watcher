# Manual Integration Testing Guide

This document describes the manual end-to-end testing procedure for claude-watcher. This requires physical hardware (ESP32 + T-Display S3) and Arduino IDE.

## Prerequisites

Before starting, ensure:
- ESP32 flashed with `claude_watcher.ino` (Task 8)
- macOS hooks installed via `macos/install.sh` (Task 4)
- pyserial installed: `pip3 install -r macos/requirements.txt`

## Step 1: Verify IDLE state on boot

After flashing the ESP32, power it on and observe the display.

**Expected display:**
- Dimmed crab (#8B5E45), closed horizontal-line eyes
- Floating `z` characters drifting upward slowly
- Status bar: **IDLE** label in blue-grey, `sleeping...` below separator line

## Step 2: Test WORKING state manually

From the project root, send a WORKING state command:

```bash
cd /path/to/claude-watcher
python3 -c "
import sys
sys.path.insert(0, 'macos')
import serial_send
serial_send.send('WORKING:Bash')
"
```

**Expected display:**
- Crab in orange (#CD7F55), open eyes
- Legs animating (alternating every 300ms)
- Status bar: **WORKING** + green spinner cycling `| / - \` + `Bash`

## Step 3: Test WAITING state manually

```bash
python3 -c "
import sys
sys.path.insert(0, 'macos')
import serial_send
serial_send.send('WAITING')
"
```

**Expected display:**
- Crab with raised claws at top corners, orange eyes
- `!` blinking above head every 500ms
- Status bar: **WAITING** in amber + `needs your input`

## Step 4: Test WAITING_URGENT state

```bash
python3 -c "
import sys
sys.path.insert(0, 'macos')
import serial_send
serial_send.send('WAITING_URGENT')
"
```

**Expected display:** Same as WAITING (same visual state, no visual distinction)

## Step 5: Test auto-idle transition

This step verifies that the ESP32 automatically transitions back to IDLE after the timeout.

For a quick test:
1. Edit `esp32/claude_watcher.ino` and temporarily change `IDLE_TIMEOUT_MS` to `10000` (10 seconds)
2. Reflash the ESP32
3. Send a WORKING state command
4. Wait 10 seconds and observe the display transition back to IDLE
5. Restore `IDLE_TIMEOUT_MS` to `5UL * 60UL * 1000UL` (5 minutes) and reflash

## Step 6: Test live Claude Code session

Start an interactive Claude Code session and ask Claude to run a command.

**Expected behavior:**
- Display shows WORKING + tool name while Claude runs tools
- Display shows WAITING after Claude finishes its turn
- Display returns to WORKING on next tool call

## Step 7: Run Python unit tests

Verify all Python tests still pass:

```bash
cd macos
python3 -m pytest tests/ -v
```

**Expected result:** 13 tests pass (9 serial_send + 4 hooks)

---

## Troubleshooting

### Serial connection issues
- Ensure the ESP32 is properly connected via USB
- Verify the correct COM port is being used (check `macos/serial_send.py`)
- Try power-cycling the ESP32

### Display not responding
- Verify the Arduino IDE flashing was successful (watch for "Done uploading" message)
- Check that the USB cable supports data transfer (some cables are power-only)

### State transitions not working
- Verify the macOS hooks are properly installed and the PATH is configured correctly
- Check that `serial_send.py` can reach the ESP32 port (try a simple echo test first)

