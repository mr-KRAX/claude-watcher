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
