# macos/ClaudeWatcher/app.py
import json
import queue

import rumps

from ClaudeWatcher.ble_manager import BLEManager
from ClaudeWatcher.ipc_server import IPCServer

_STATE_ICONS = {
    "WORKING": "🦀",
    "WAITING": "👀",
    "IDLE":    "💤",
}

# Maps hook event name → (display state, BLE message)
_HOOK_MAP = {
    "Stop":              ("IDLE",    "IDLE"),
    "Notification":      ("WAITING", "WAITING"),
    "PermissionRequest": ("WAITING", "WAITING"),
    "PostToolUse":       ("WORKING", "WORKING"),
    "UserPromptSubmit":  ("WORKING", "WORKING"),
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
        try:
            data = json.loads(msg)
        except (json.JSONDecodeError, ValueError):
            return

        hook       = data.get("hook", "")
        session_id = data.get("session_id", "—")
        sid        = session_id[:8] if session_id != "—" else "—"

        if hook == "PreToolUse":
            tool = data.get("tool_name", "")
            print(f"[app] {hook} tool={tool} session={sid}")
            self._state_label = "WORKING"
            self._tool_label  = tool
            ble_msg = f"WORKING:{tool}" if tool else "WORKING"
        elif hook in _HOOK_MAP:
            state, ble_msg = _HOOK_MAP[hook]
            print(f"[app] {hook} → {ble_msg} session={sid}")
            self._state_label = state
            self._tool_label  = ""
        else:
            print(f"[app] unknown hook={hook!r} session={sid}")
            return

        # Keep only the latest message in the queue
        try:
            self._msg_queue.get_nowait()
        except queue.Empty:
            pass
        self._msg_queue.put_nowait(ble_msg)

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
