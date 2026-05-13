# macos/ClaudeWatcher/ble_manager.py
import asyncio
import queue
import threading

from bleak import BleakClient, BleakScanner

from ClaudeWatcher.config import BLE_RETRY_INTERVAL, CHAR_UUID, SERVICE_UUID


class BLEManager:
    def __init__(self, msg_queue: queue.Queue, on_status_change, retry_interval=None):
        self._queue = msg_queue
        self._on_status = on_status_change
        self._retry = retry_interval if retry_interval is not None else BLE_RETRY_INTERVAL

    def start(self):
        t = threading.Thread(target=self._run, daemon=True)
        t.start()

    def _run(self):
        asyncio.run(self._ble_loop())

    async def _ble_loop(self):
        while True:
            self._on_status("Scanning…")
            device = await BleakScanner.find_device_by_filter(
                lambda d, adv: SERVICE_UUID.lower() in [
                    s.lower() for s in adv.service_uuids
                ],
                timeout=5.0,
            )
            if device is not None:
                client = BleakClient(device)
                try:
                    await client.connect()
                    self._on_status("Connected")
                    await self._send_loop(client)
                except Exception:
                    pass
                finally:
                    self._on_status("Reconnecting…")
                    try:
                        await client.disconnect()
                    except Exception:
                        pass
            await asyncio.sleep(self._retry)

    async def _send_loop(self, client: BleakClient):
        while client.is_connected:
            try:
                msg = self._queue.get_nowait()
                await client.write_gatt_char(CHAR_UUID, msg.encode("utf-8"), response=False)
            except queue.Empty:
                await asyncio.sleep(0.05)
