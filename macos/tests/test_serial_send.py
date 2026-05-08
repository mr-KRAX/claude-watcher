import json
import os
import sys
import pytest
from unittest.mock import patch, MagicMock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import serial_send


def test_find_port_from_config(tmp_path, monkeypatch):
    config = tmp_path / "config.json"
    config.write_text(json.dumps({"serial_port": "/dev/cu.usbmodem1301"}))
    monkeypatch.setattr(serial_send, "CONFIG_PATH", str(config))
    assert serial_send.find_port() == "/dev/cu.usbmodem1301"


def test_find_port_config_empty_string_falls_through(tmp_path, monkeypatch):
    config = tmp_path / "config.json"
    config.write_text(json.dumps({"serial_port": ""}))
    monkeypatch.setattr(serial_send, "CONFIG_PATH", str(config))
    with patch("glob.glob") as mock_glob:
        mock_glob.side_effect = [["/dev/cu.usbmodem1301"], [], []]
        assert serial_send.find_port() == "/dev/cu.usbmodem1301"


def test_find_port_autodetect_usbmodem(tmp_path, monkeypatch):
    monkeypatch.setattr(serial_send, "CONFIG_PATH", str(tmp_path / "nofile.json"))
    with patch("glob.glob") as mock_glob:
        mock_glob.side_effect = [["/dev/cu.usbmodem1301"], [], []]
        assert serial_send.find_port() == "/dev/cu.usbmodem1301"


def test_find_port_fallback_usbserial(tmp_path, monkeypatch):
    monkeypatch.setattr(serial_send, "CONFIG_PATH", str(tmp_path / "nofile.json"))
    with patch("glob.glob") as mock_glob:
        mock_glob.side_effect = [[], ["/dev/cu.usbserial-ABC"], []]
        assert serial_send.find_port() == "/dev/cu.usbserial-ABC"


def test_find_port_fallback_slab(tmp_path, monkeypatch):
    monkeypatch.setattr(serial_send, "CONFIG_PATH", str(tmp_path / "nofile.json"))
    with patch("glob.glob") as mock_glob:
        mock_glob.side_effect = [[], [], ["/dev/cu.SLAB_USBtoUART"]]
        assert serial_send.find_port() == "/dev/cu.SLAB_USBtoUART"


def test_find_port_none_found(tmp_path, monkeypatch):
    monkeypatch.setattr(serial_send, "CONFIG_PATH", str(tmp_path / "nofile.json"))
    with patch("glob.glob", return_value=[]):
        assert serial_send.find_port() is None


def test_send_writes_newline_terminated_message():
    with patch.object(serial_send, "find_port", return_value="/dev/cu.usbmodem1301"):
        mock_ser = MagicMock()
        with patch("serial.Serial") as MockSerial:
            MockSerial.return_value.__enter__ = lambda s: mock_ser
            MockSerial.return_value.__exit__ = MagicMock(return_value=False)
            serial_send.send("WORKING:Bash")
            mock_ser.write.assert_called_once_with(b"WORKING:Bash\n")


def test_send_silent_fail_when_no_port(capsys):
    with patch.object(serial_send, "find_port", return_value=None):
        serial_send.send("WORKING:Bash")  # must not raise
    captured = capsys.readouterr()
    assert "no serial port" in captured.err


def test_send_silent_fail_on_serial_error(capsys):
    with patch.object(serial_send, "find_port", return_value="/dev/cu.usbmodem1301"):
        with patch("serial.Serial", side_effect=Exception("port busy")):
            serial_send.send("WORKING:Bash")  # must not raise
    captured = capsys.readouterr()
    assert "serial error" in captured.err
