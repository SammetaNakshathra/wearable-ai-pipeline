# Wearable AI Vision & Audio Pipeline

A prototype architecture for an AI-powered wearable system using an **ESP32-S3 camera + I2S MEMS microphone** connected to a **local Raspberry Pi AI Hub**.

The system demonstrates reliable image/audio transmission, non-blocking audio acquisition using DMA and FreeRTOS, and asynchronous AI inference with intermediate processing feedback.

---

## 1. Project Overview

This project addresses the design of a wearable AI pipeline where an ESP32-S3 captures visual and audio information and sends it to a local Raspberry Pi-based AI hub.

The system is designed around three key requirements:

1. Reliable communication between the ESP32-S3 wearable and Raspberry Pi AI Hub.
2. Continuous I2S microphone acquisition using DMA and ping-pong buffering without blocking network communication.
3. An asynchronous Python AI hub capable of handling slow local AI inference while immediately returning a processing status to the wearable.

### High-Level Architecture

```text
        ┌─────────────────────────────┐
        │        ESP32-S3             │
        │                             │
        │  Camera ───────┐            │
        │                │            │
        │  I2S Mic ──> DMA Buffers    │
        │                │            │
        │        FreeRTOS Tasks        │
        └───────────┬─────────────────┘
                    │
              WebSocket / TCP
                    │
                    ▼
        ┌─────────────────────────────┐
        │      Raspberry Pi Hub       │
        │                             │
        │       FastAPI Server        │
        │             │               │
        │       Async Processing      │
        │             │               │
        │      Local VLM / LLM        │
        │             │               │
        │       Processing State      │
        │             │               │
        │       Final Response        │
        └─────────────┬───────────────┘
                      │
                WebSocket Response
                      │
                      ▼
                 ESP32-S3
```

---

## 2. Repository Structure

```text
wearable-ai-pipeline/
│
├── README.md
├── DESIGN.md
│
├── esp32/
│   └── audio_capture.cpp
│
└── pi_hub/
    ├── server.py
    └── mock_glasses_client.py
```

---

## 3. Key Design Decisions

### Communication Protocol

**WebSocket over TCP** was selected for communication between the ESP32-S3 and Raspberry Pi.

Reasons:

* Reliable and ordered delivery through TCP.
* Persistent bidirectional connection.
* Convenient message framing.
* Supports server-to-device status messages.
* Suitable for sending image/audio requests and receiving AI responses.
* Avoids manual packet-loss recovery and packet ordering logic required with UDP.

UDP can provide lower transport overhead, but packet loss, ordering, fragmentation and retransmission would require additional application-level handling.

---

## 4. Audio Capture Design

The ESP32-S3 uses an I2S MEMS microphone.

The audio pipeline is:

```text
I2S MEMS Microphone
        ↓
I2S Peripheral
        ↓
DMA
        ↓
Ping-Pong Audio Buffers
        ↓
FreeRTOS Queue
        ↓
Audio Processing Task
        ↓
Network Transmission
```

Two application-side buffers are used so that completed audio blocks can be processed while the next block is acquired.

The audio capture task does not perform blocking network operations.

This separation prevents network latency from directly interrupting continuous audio acquisition.

---

## 5. DMA and Ping-Pong Buffering

The ESP32-S3 I2S peripheral uses DMA for continuous audio acquisition.

The application uses two alternating buffers:

```text
        ┌───────────────┐
        │   Buffer 0    │
        └───────────────┘
                ↑
          Capture / Process

        ┌───────────────┐
        │   Buffer 1    │
        └───────────────┘
                ↑
          Process / Capture
```

With this approach, the system can alternate between buffers rather than waiting for an entire audio processing operation to finish before continuing acquisition.

The implementation is provided in:

```text
esp32/audio_capture.cpp
```

> Note: The ESP32 implementation is provided as a hardware-side ESP-IDF reference implementation. GPIO assignments and exact API configuration should be adapted and validated for the target ESP32-S3 development board and ESP-IDF version.

---

## 6. FreeRTOS Task Separation

The ESP32 design separates audio capture from processing/network activity.

### Audio Capture Task

Responsible for:

* Reading audio samples from I2S.
* Receiving completed DMA blocks.
* Placing completed buffers into a FreeRTOS queue.

### Audio Processing Task

Responsible for:

* Consuming completed audio buffers.
* Preprocessing or compressing audio.
* Preparing data for network transmission.

### Network Task

In the complete hardware implementation, network communication should operate independently from the audio capture task.

This prevents a slow network operation from blocking microphone acquisition.

---

## 7. Raspberry Pi AI Hub

The Raspberry Pi side is implemented using:

* Python
* FastAPI
* WebSockets
* asyncio

The hub exposes a WebSocket endpoint:

```text
/ws
```

The ESP32 sends a request containing:

```json
{
    "request_id": "unique-request-id",
    "image": "<base64-image>",
    "audio": "<base64-audio>"
}
```

The hub:

1. Accepts the WebSocket connection.
2. Receives the request.
3. Validates the image and audio payload.
4. Decodes Base64 data.
5. Immediately returns a `processing` state.
6. Runs the AI inference function asynchronously.
7. Returns the final result.

---

## 8. Handling Slow AI Inference

Local AI inference may take several seconds depending on the model and Raspberry Pi hardware.

The hub therefore does not immediately block the wearable waiting for inference.

Instead, it sends:

```json
{
    "status": "processing",
    "request_id": "unique-request-id",
    "message": "Processing your request..."
}
```

After inference completes:

```json
{
    "status": "done",
    "request_id": "unique-request-id",
    "text": "The text on the label is: Organic Green Tea."
}
```

The current implementation uses a mock VLM with an artificial delay to demonstrate this behavior.

---

## 9. Error Handling

The prototype handles:

* Missing image payload.
* Missing audio payload.
* Invalid Base64 data.
* WebSocket disconnection.
* Unexpected server errors.

Example error response:

```json
{
    "status": "error",
    "request_id": "unique-request-id",
    "message": "Image or audio payload missing."
}
```

---

## 10. Running the Prototype

### Install Python Dependencies

From the `pi_hub` directory:

```bash
pip install fastapi uvicorn websockets
```

### Start the AI Hub

```bash
python server.py
```

The server runs on:

```text
http://127.0.0.1:8000
```

### Test the WebSocket Client

Open another terminal:

```bash
python mock_glasses_client.py
```

Expected output:

```text
[ESP32] Connecting to AI Hub...
[ESP32] Connected to AI Hub.
[ESP32] Sending image + audio...
[ESP32] Hub response: {
    'status': 'processing',
    ...
}
[ESP32] Hub response: {
    'status': 'done',
    ...
}
```

---

## 11. Demonstration

The current software prototype demonstrates:

* ESP32-to-hub communication using WebSockets.
* Image and audio payload transmission.
* Request ID tracking.
* Intermediate `processing` response.
* Simulated slow AI inference.
* Final AI response.
* Error handling.
* Asynchronous Python server architecture.

The WebSocket communication is currently demonstrated using a Python mock glasses client on Windows.

The ESP32 audio capture implementation is provided as an ESP-IDF reference implementation and has not been represented as hardware-tested in this prototype.

---

## 12. Future Improvements

The prototype can be extended with:

* Actual ESP32-S3 camera integration.
* Physical I2S MEMS microphone integration.
* Raspberry Pi hardware deployment.
* Local Whisper or another speech recognition model.
* Local vision-language model integration.
* JPEG image compression on ESP32.
* Audio compression or voice activity detection.
* Streaming audio instead of sending complete buffers.
* Authentication and encrypted WebSocket communication.
* Request cancellation and timeout handling.
* Multiple concurrent wearable clients.
* Persistent inference queue.
* Hardware-level latency and packet-loss benchmarking.

---

## 13. Technologies

### Embedded

* ESP32-S3
* ESP-IDF
* I2S
* DMA
* FreeRTOS

### Communication

* WebSocket
* TCP
* JSON
* Base64

### AI Hub

* Python
* FastAPI
* asyncio
* Uvicorn

### AI

* Local LLM/VLM architecture
* Mock VLM for prototype demonstration

---

## 14. Demo Links

### GitHub

`<https://github.com/SammetaNakshathra/wearable-ai-pipeline>`

### Demo Video

`<https://drive.google.com/drive/folders/1Wjzl7tttbr-8cDeVqk-aG2US8hIXWCSw>`

---

## 15. Project Status

**Prototype / Take-Home Challenge Implementation**

The Raspberry Pi hub and WebSocket communication flow have been implemented and tested using a Python mock wearable client.

The ESP32-S3 audio capture component is provided as an ESP-IDF hardware reference implementation. Physical ESP32-S3 hardware validation remains a next step because GPIO configuration and board-specific I2S setup depend on the target hardware.
