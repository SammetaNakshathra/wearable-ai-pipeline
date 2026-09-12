import asyncio
import base64
import json
import uuid

import websockets


HUB_URL = "ws://127.0.0.1:8000/ws"


async def main():
    print("[ESP32] Connecting to AI Hub...")

    async with websockets.connect(HUB_URL) as websocket:
        print("[ESP32] Connected to AI Hub.")

        # Simulated image and audio data
        image_data = b"fake-camera-image-data"
        audio_data = b"fake-i2s-audio-data"

        request = {
            "request_id": str(uuid.uuid4()),
            "image": base64.b64encode(image_data).decode("utf-8"),
            "audio": base64.b64encode(audio_data).decode("utf-8")
        }

        print("[ESP32] Sending image + audio...")

        await websocket.send(json.dumps(request))

        while True:
            response = await websocket.recv()
            response = json.loads(response)

            print(f"[ESP32] Hub response: {response}")

            if response.get("status") in ["done", "error"]:
                break


if __name__ == "__main__":
    asyncio.run(main())