import asyncio
import websockets
from datetime import datetime
from arduino.app_utils import App

# https://intergular-laila-unmeaningly.ngrok-free.dev izzy's url
# https://tona-citizenly-wrinkledly.ngrok-free.dev nav's url

DEVICE_NAME = "giver"  # change to "arduino2" on the other device
SERVER_URI = "wss://tona-citizenly-wrinkledly.ngrok-free.dev/ws"
RECONNECT_DELAY = 5

async def send_loop(ws):
    while True:
        msg = f"hello from {DEVICE_NAME}"
        await ws.send(msg)
        print(f"[{timestamp()}] 📤 Sent: {msg}")
        await asyncio.sleep(5)

async def receive_loop(ws):
    async for message in ws:
        print(f"[{timestamp()}] 📨 Received: {message}")

async def listen():
    while True:
        try:
            print(f"[{timestamp()}] Connecting to {SERVER_URI}...")
            async with websockets.connect(SERVER_URI) as ws:
                print(f"[{timestamp()}] ✓ Connected as {DEVICE_NAME}!")
                await asyncio.gather(
                    send_loop(ws),
                    receive_loop(ws)
                )
        except websockets.exceptions.ConnectionClosedError as e:
            print(f"[{timestamp()}] ✗ Connection closed: {e}")
        except ConnectionRefusedError:
            print(f"[{timestamp()}] ✗ Connection refused — is the server running?")
        except Exception as e:
            print(f"[{timestamp()}] ✗ Error: {e}")
        print(f"[{timestamp()}] Retrying in {RECONNECT_DELAY}s...")
        await asyncio.sleep(RECONNECT_DELAY)

def timestamp():
    return datetime.now().strftime("%H:%M:%S")

def loop():
    asyncio.run(listen())

App.run(user_loop=loop)