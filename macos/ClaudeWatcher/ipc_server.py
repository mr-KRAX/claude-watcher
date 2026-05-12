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
