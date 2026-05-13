# macos/ipc_send.py
import json
import socket
import sys

SOCK_PATH = "/tmp/claude-watcher.sock"


def send(hook: str, session_id: str = "", **kwargs):
    """Send a hook event to ClaudeWatcher.app via Unix socket. Silent fail."""
    payload = {"hook": hook, "session_id": session_id, **kwargs}
    sid = session_id[:8] if session_id else "—"
    extra = f" tool={kwargs['tool_name']}" if "tool_name" in kwargs else ""
    print(f"[claude-watcher] → {hook}{extra} (session: {sid})")
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(SOCK_PATH)
            s.sendall(json.dumps(payload).encode("utf-8"))
    except Exception as e:
        print(f"[claude-watcher] error: {e}", file=sys.stderr)
