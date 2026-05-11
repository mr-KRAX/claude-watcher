import glob
import json
import os
import sys

try:
    import serial
    _serial_available = True
except ImportError:
    _serial_available = False

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "config.json")
BAUD_RATE = 115200
TIMEOUT = 2


def find_port():
    """Resolve serial port: config file → usbmodem* → usbserial-* → SLAB_*"""
    if os.path.exists(CONFIG_PATH):
        try:
            with open(CONFIG_PATH) as f:
                config = json.load(f)
            port = config.get("serial_port", "")
            if port:
                return port
        except (json.JSONDecodeError, IOError):
            pass

    for pattern in ["/dev/cu.usbmodem*", "/dev/cu.usbserial-*", "/dev/cu.SLAB_*"]:
        matches = glob.glob(pattern)
        if matches:
            return matches[0]

    return None


def send(message):
    """Send a newline-terminated message to the ESP32. Silent fail if no port or pyserial missing."""
    if not _serial_available:
        print("claude-watcher: pyserial not installed (pip install pyserial)", file=sys.stderr)
        return
    port = find_port()
    if not port:
        print("claude-watcher: no serial port found", file=sys.stderr)
        return

    try:
        with serial.Serial(port, BAUD_RATE, timeout=TIMEOUT) as ser:
            ser.write(f"{message}\n".encode("utf-8"))
    except Exception as e:
        print(f"claude-watcher: serial error: {e}", file=sys.stderr)
