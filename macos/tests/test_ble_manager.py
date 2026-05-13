# macos/tests/test_ble_manager.py
import asyncio
import os
import queue
import sys
import time
from unittest.mock import AsyncMock, MagicMock, patch

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from ClaudeWatcher.ble_manager import BLEManager


def test_scanning_status_emitted_when_no_device():
    """Status callback receives 'Scanning…' when no BLE device is found."""
    statuses = []
    q = queue.Queue(maxsize=1)

    with patch("ClaudeWatcher.ble_manager.BleakScanner") as MockScanner:
        MockScanner.find_device_by_filter = AsyncMock(return_value=None)
        mgr = BLEManager(q, statuses.append, retry_interval=0.05)
        mgr.start()
        time.sleep(0.3)

    assert "Scanning…" in statuses


def test_write_called_with_queued_message():
    """BLEManager writes queued message bytes to the BLE characteristic."""
    written = []
    statuses = []
    q = queue.Queue(maxsize=1)
    q.put("WORKING:Bash")

    mock_device = MagicMock()
    mock_client = AsyncMock()
    mock_client.is_connected = True

    async def mock_write(char_uuid, data, response=False):
        written.append(data)
        mock_client.is_connected = False  # stop after first write

    mock_client.connect = AsyncMock()
    mock_client.disconnect = AsyncMock()
    mock_client.write_gatt_char = mock_write

    with patch("ClaudeWatcher.ble_manager.BleakScanner") as MockScanner, \
         patch("ClaudeWatcher.ble_manager.BleakClient", return_value=mock_client):
        MockScanner.find_device_by_filter = AsyncMock(return_value=mock_device)
        mgr = BLEManager(q, statuses.append, retry_interval=0.05)
        mgr.start()
        time.sleep(0.5)

    assert b"WORKING:Bash" in written


def test_connected_status_emitted_on_connect():
    """Status callback receives 'Connected' after a successful BLE connection."""
    statuses = []
    q = queue.Queue(maxsize=1)

    mock_device = MagicMock()
    mock_client = AsyncMock()
    mock_client.is_connected = False  # immediately disconnect to end send loop

    mock_client.connect = AsyncMock()
    mock_client.disconnect = AsyncMock()
    mock_client.write_gatt_char = AsyncMock()

    with patch("ClaudeWatcher.ble_manager.BleakScanner") as MockScanner, \
         patch("ClaudeWatcher.ble_manager.BleakClient", return_value=mock_client):
        MockScanner.find_device_by_filter = AsyncMock(return_value=mock_device)
        mgr = BLEManager(q, statuses.append, retry_interval=0.5)
        mgr.start()
        time.sleep(0.3)

    assert "Connected" in statuses
