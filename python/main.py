import asyncio
import json
import queue
from datetime import datetime

import websockets
from arduino.app_utils import App, Bridge

DEVICE_NAME = "giver"
SERVER_URI = "wss://tona-citizenly-wrinkledly.ngrok-free.dev/ws"
RECONNECT_DELAY = 5

# thread-safe queue for packets coming from the MCU
outgoing_packets = queue.Queue()


def timestamp():
    return datetime.now().strftime("%H:%M:%S")


def on_sensor_packet(payload):
    """
    Called by RouterBridge when the Arduino sketch does:
    Bridge.notify("sensor_packet", some_string)
    """
    try:
        data = json.loads(payload)
    except Exception:
        data = {"raw": str(payload)}

    data["device"] = DEVICE_NAME
    data["mpu_time"] = datetime.now().isoformat(timespec="seconds")
    outgoing_packets.put(data)
    print(f"[{timestamp()}] queued sensor packet: {data}")


async def send_loop(ws):
    while True:
        # wait for the next packet from Arduino in a background thread
        packet = await asyncio.to_thread(outgoing_packets.get)

        msg = json.dumps(packet)
        await ws.send(msg)
        print(f"[{timestamp()}] 📤 Sent: {msg}")


async def receive_loop(ws):
    async for message in ws:
        print(f"[{timestamp()}] 📨 Received: {message}")


async def listen():
    while True:
        try:
            print(f"[{timestamp()}] Connecting to {SERVER_URI}...")
            async with websockets.connect(SERVER_URI) as ws:
                print(f"[{timestamp()}] ✓ Connected as {DEVICE_NAME}!")

                # optional: tell server who connected
                hello = {
                    "type": "hello",
                    "device": DEVICE_NAME,
                    "time": datetime.now().isoformat(timespec="seconds"),
                }
                await ws.send(json.dumps(hello))

                await asyncio.gather(
                    send_loop(ws),
                    receive_loop(ws),
                )

        except websockets.exceptions.ConnectionClosedError as e:
            print(f"[{timestamp()}] ✗ Connection closed: {e}")
        except ConnectionRefusedError:
            print(f"[{timestamp()}] ✗ Connection refused — is the server running?")
        except Exception as e:
            print(f"[{timestamp()}] ✗ Error: {e}")

        print(f"[{timestamp()}] Retrying in {RECONNECT_DELAY}s...")
        await asyncio.sleep(RECONNECT_DELAY)


def loop():
    Bridge.provide("sensor_packet", on_sensor_packet)
    asyncio.run(listen())


App.run(user_loop=loop)