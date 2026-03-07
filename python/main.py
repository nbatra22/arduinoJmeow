import asyncio
import websockets
import json
from datetime import datetime

# https://intergular-laila-unmeaningly.ngrok-free.dev

SERVER_URI = "wss://intergular-laila-unmeaningly.ngrok-free.dev/ws" #change per program
RECONNECT_DELAY = 5  # seconds

async def listen():
    while True:
        try:
            print(f"[{timestamp()}] Connecting to {SERVER_URI}...")
            async with websockets.connect(SERVER_URI) as ws:
                print(f"[{timestamp()}] ✓ WebSocket connected!")
                async for message in ws:
                    handle_message(message)

        except websockets.exceptions.ConnectionClosedError as e:
            print(f"[{timestamp()}] ✗ Connection closed: {e}")
        except ConnectionRefusedError:
            print(f"[{timestamp()}] ✗ Connection refused — is the server running?")
        except Exception as e:
            print(f"[{timestamp()}] ✗ Error: {e}")

        print(f"[{timestamp()}] Retrying in {RECONNECT_DELAY}s...")
        await asyncio.sleep(RECONNECT_DELAY)

def handle_message(message):
    print(f"[{timestamp()}] 📨 Received: {message}")
    # Try to parse as JSON, otherwise treat as plain string
    try:
        data = json.loads(message)
        print(f"           Parsed JSON: {data}")
    except json.JSONDecodeError:
        pass  # plain string, already printed above

def timestamp():
    return datetime.now().strftime("%H:%M:%S")

if __name__ == "__main__":
    try:
        asyncio.run(listen())
    except KeyboardInterrupt:
        print("\nStopped.")