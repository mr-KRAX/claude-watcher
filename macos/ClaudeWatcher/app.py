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
