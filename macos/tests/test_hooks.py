import json
import os
import sys
from io import StringIO
from unittest.mock import patch, call

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))


def test_on_pre_tool_sends_working_with_tool_name():
    import on_pre_tool
    import serial_send
    payload = json.dumps({"hook_event_name": "PreToolUse", "tool_name": "Bash"})
    with patch.object(serial_send, "send") as mock_send:
        with patch("sys.stdin", StringIO(payload)):
            on_pre_tool.main()
        mock_send.assert_called_once_with("WORKING:Bash")


def test_on_pre_tool_handles_unknown_tool():
    import on_pre_tool
    import serial_send
    payload = json.dumps({"hook_event_name": "PreToolUse"})
    with patch.object(serial_send, "send") as mock_send:
        with patch("sys.stdin", StringIO(payload)):
            on_pre_tool.main()
        mock_send.assert_called_once_with("WORKING:Unknown")


def test_on_stop_sends_waiting():
    import on_stop
    import serial_send
    with patch.object(serial_send, "send") as mock_send:
        with patch("sys.stdin", StringIO("{}")):
            on_stop.main()
        mock_send.assert_called_once_with("WAITING")


def test_on_notification_sends_waiting_urgent():
    import on_notification
    import serial_send
    with patch.object(serial_send, "send") as mock_send:
        with patch("sys.stdin", StringIO("{}")):
            on_notification.main()
        mock_send.assert_called_once_with("WAITING_URGENT")
