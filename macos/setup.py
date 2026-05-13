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
