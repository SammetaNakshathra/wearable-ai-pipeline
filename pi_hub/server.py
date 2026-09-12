import asyncio
import base64
import json
import uuid
from datetime import datetime

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import uvicorn


app = FastAPI(
    title="Wearable AI Vision & Audio Pipeline",
    description="Raspberry Pi AI Hub for ESP32-S3 smart glasses"
)


# ---------------------------------------------------------
# Mock VLM
# ---------------------------------------------------------
async def mock_vlm(image_data: bytes, audio_data: bytes) -> str:
    """
    Simulates a local Vision-Language Model.

    In the real system, this function would send the image
    and audio to a locally hosted VLM/LLM.
    """

    print("[VLM] Starting inference...")

    # Simulate slow inference
    await asyncio.sleep(5)

    print("[VLM] Inference completed.")

    return "The text on the label is: Organic Green Tea."


# ---------------------------------------------------------
# Health Check
# ---------------------------------------------------------
@app.get("/")
async def root():
    return {
        "service": "Wearable AI Vision & Audio Pipeline",
        "status": "running"
    }


# ---------------------------------------------------------
# WebSocket Endpoint
# ---------------------------------------------------------
@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):

    await websocket.accept()

    print("[HUB] ESP32 client connected.")

    try:

        # -------------------------------------------------
        # Receive request
        # -------------------------------------------------
        message = await websocket.receive_text()

        request = json.loads(message)

        request_id = request.get(
            "request_id",
            str(uuid.uuid4())
        )

        image_b64 = request.get("image")
        audio_b64 = request.get("audio")

        if not image_b64 or not audio_b64:

            await websocket.send_json({
                "status": "error",
                "request_id": request_id,
                "message": "Image or audio payload missing."
            })

            return

        print(f"[HUB] Received request: {request_id}")

        # -------------------------------------------------
        # Decode image and audio
        # -------------------------------------------------
        try:

            image_data = base64.b64decode(image_b64)
            audio_data = base64.b64decode(audio_b64)

        except Exception:

            await websocket.send_json({
                "status": "error",
                "request_id": request_id,
                "message": "Invalid Base64 payload."
            })

            return

        print(
            f"[HUB] Image: {len(image_data)} bytes | "
            f"Audio: {len(audio_data)} bytes"
        )

        # -------------------------------------------------
        # Immediately notify the glasses
        # -------------------------------------------------
        await websocket.send_json({
            "status": "processing",
            "request_id": request_id,
            "message": "Processing your request..."
        })

        print("[HUB] Sent: processing")

        # -------------------------------------------------
        # Run VLM asynchronously
        # -------------------------------------------------
        result = await mock_vlm(
            image_data,
            audio_data
        )

        # -------------------------------------------------
        # Send final response
        # -------------------------------------------------
        await websocket.send_json({
            "status": "done",
            "request_id": request_id,
            "text": result,
            "timestamp": datetime.now().isoformat()
        })

        print("[HUB] Sent: done")

    except WebSocketDisconnect:

        print("[HUB] ESP32 client disconnected.")

    except Exception as e:

        print(f"[HUB] Error: {e}")

        try:
            await websocket.send_json({
                "status": "error",
                "message": "Internal server error."
            })
        except Exception:
            pass


# ---------------------------------------------------------
# Run Server
# ---------------------------------------------------------
if __name__ == "__main__":

    uvicorn.run(
        app,
        host="0.0.0.0",
        port=8000
    )