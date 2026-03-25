# main.py
import sys
from pathlib import Path

# Ensure parent modules are importable
_parent = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(_parent))

from contextlib import asynccontextmanager
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from api import devices, tasks, records, ftp, config
from services.ws_manager import ws_manager
from services.state import state
from services import usb_monitor_service, copy_engine_service, ftp_uploader_service, stats_service


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    usb_monitor_service.start_usb_monitor()
    copy_engine_service.init_copy_engine()
    ftp_uploader_service.init_ftp_uploader()
    stats_service.start_stats_broadcast(interval=5.0)
    state.started = True
    yield
    # Shutdown
    state.started = False


app = FastAPI(title="USB AutoDump API", version="1.0.0", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(devices.router)
app.include_router(tasks.router)
app.include_router(records.router)
app.include_router(ftp.router)
app.include_router(config.router)


@app.get("/health")
def health():
    return {"status": "ok", "started": state.started}


@app.websocket("/ws/progress")
async def ws_progress(ws: WebSocket):
    await ws_manager.connect_progress(ws)
    try:
        while True:
            data = await ws.receive_text()
            if data == "ping":
                await ws.send_text("pong")
    except WebSocketDisconnect:
        await ws_manager.disconnect(ws)


@app.websocket("/ws/devices")
async def ws_devices(ws: WebSocket):
    await ws_manager.connect_devices(ws)
    try:
        while True:
            data = await ws.receive_text()
            if data == "ping":
                await ws.send_text("pong")
    except WebSocketDisconnect:
        await ws_manager.disconnect(ws)


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
