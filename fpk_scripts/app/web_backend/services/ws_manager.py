# services/ws_manager.py
import asyncio
import json
from typing import Any, Dict, Set
from fastapi import WebSocket


class WSManager:
    def __init__(self):
        self._progress_clients: Set[WebSocket] = set()
        self._device_clients: Set[WebSocket] = set()
        self._lock = asyncio.Lock()

    async def connect_progress(self, ws: WebSocket):
        await ws.accept()
        async with self._lock:
            self._progress_clients.add(ws)

    async def connect_devices(self, ws: WebSocket):
        await ws.accept()
        async with self._lock:
            self._device_clients.add(ws)

    async def disconnect(self, ws: WebSocket):
        async with self._lock:
            self._progress_clients.discard(ws)
            self._device_clients.discard(ws)

    async def _send(self, clients: Set[WebSocket], msg: str):
        dead = set()
        for ws in clients:
            try:
                await ws.send_text(msg)
            except Exception:
                dead.add(ws)
        if dead:
            async with self._lock:
                clients -= dead

    async def broadcast_copy_progress(self, data: Dict[str, Any]):
        data["type"] = "copy_progress"
        msg = json.dumps(data)
        async with self._lock:
            await self._send(self._progress_clients, msg)

    async def broadcast_upload_queue(self, data: Dict[str, Any]):
        data["type"] = "upload_queue"
        msg = json.dumps(data)
        async with self._lock:
            await self._send(self._progress_clients, msg)

    async def broadcast_stats(self, data: Dict[str, Any]):
        data["type"] = "stats"
        msg = json.dumps(data)
        async with self._lock:
            await self._send(self._progress_clients, msg)
            await self._send(self._device_clients, msg)

    async def broadcast_device(self, data: Dict[str, Any]):
        msg = json.dumps({"type": "device", **data})
        async with self._lock:
            await self._send(self._device_clients, msg)


ws_manager = WSManager()
