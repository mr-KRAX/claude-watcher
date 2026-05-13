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
