import json
import os
import sys
from io import StringIO
from unittest.mock import patch

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))


def _run(module_name, stdin_data):
    import importlib
    mod = importlib.import_module(module_name)
    import ipc_send
    with patch.object(ipc_send, "send") as mock_send:
        with patch("sys.stdin", StringIO(stdin_data)):
            mod.main()
    return mock_send


def test_on_pre_tool_sends_hook_with_tool_name():
    mock = _run("on_pre_tool", json.dumps({"session_id": "sess-1", "tool_name": "Bash"}))
    mock.assert_called_once_with("PreToolUse", session_id="sess-1", tool_name="Bash")


def test_on_pre_tool_defaults_unknown_tool():
    mock = _run("on_pre_tool", json.dumps({"session_id": "sess-2"}))
    mock.assert_called_once_with("PreToolUse", session_id="sess-2", tool_name="Unknown")


def test_on_stop_sends_hook():
    mock = _run("on_stop", json.dumps({"session_id": "sess-3"}))
    mock.assert_called_once_with("Stop", session_id="sess-3")


def test_on_notification_sends_hook():
    mock = _run("on_notification", json.dumps({"session_id": "sess-4"}))
    mock.assert_called_once_with("Notification", session_id="sess-4")


def test_on_post_tool_sends_hook():
    mock = _run("on_post_tool", json.dumps({"session_id": "sess-5"}))
    mock.assert_called_once_with("PostToolUse", session_id="sess-5")


def test_on_user_prompt_sends_hook():
    mock = _run("on_user_prompt", json.dumps({"session_id": "sess-6"}))
    mock.assert_called_once_with("UserPromptSubmit", session_id="sess-6")


def test_on_permission_request_sends_hook():
    mock = _run("on_permission_request", json.dumps({"session_id": "sess-7"}))
    mock.assert_called_once_with("PermissionRequest", session_id="sess-7")
