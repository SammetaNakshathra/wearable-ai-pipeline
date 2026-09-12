# Wearable AI Vision & Audio Pipeline — System Design

## 1. System Overview

The proposed system consists of an ESP32-S3 wearable device and a local Raspberry Pi AI hub.

The ESP32-S3 contains:

- Camera for image capture
- I2S MEMS microphone for audio capture
- Wi-Fi connectivity
- FreeRTOS tasks for concurrent processing

The Raspberry Pi acts as the local AI hub and provides:

- WebSocket communication
- Image and audio reception
- Request management
- AI/VLM inference orchestration
- Intermediate processing status
- Final response delivery

System flow:

ESP32-S3
    |
    | WebSocket over Wi-Fi
    v
Raspberry Pi AI Hub
    |
    +---- Image + Audio Processing
    |
    v
Local VLM / LLM
    |
    v
Processing Result
    |
    v
ESP32-S3


---

## 2. Communication Protocol Selection

### Selected Protocol: WebSocket over TCP

WebSocket over TCP was selected for communication between the ESP32-S3 and Raspberry Pi.

The main reasons are:

1. Reliable delivery
2. Ordered data transmission
3. Persistent connection
4. Bidirectional communication
5. Simple application-level message framing
6. Easy delivery of intermediate status messages
7. Suitable support in Python and ESP32 environments

The wearable device needs to transmit image and audio data reliably. Losing part of an image or audio request would make the AI inference unreliable.

Therefore, reliability is more important than achieving the absolute minimum network latency.

---

## 3. TCP vs UDP vs WebSocket

| Protocol | Advantages | Disadvantages | Decision |
|----------|------------|---------------|----------|
| UDP | Very low overhead and latency | Packet loss, ordering and reassembly must be handled manually | Not selected |
| TCP | Reliable and ordered delivery | Requires connection management | Suitable |
| WebSocket | Reliable TCP transport + persistent bidirectional communication + message framing | Slight protocol overhead | Selected |

UDP could be considered for future real-time streaming where occasional packet loss is acceptable.

For the current image + audio request/response workload, WebSocket provides a better balance between reliability, latency and implementation complexity.

---

## 4. ESP32-S3 Audio Pipeline

The I2S MEMS microphone continuously produces audio samples.

The ESP32-S3 uses the I2S peripheral together with DMA buffering.

Architecture:

I2S MEMS Microphone
        |
        v
I2S Peripheral
        |
        v
DMA
   +---------+
   | Buffer 0|
   +---------+
        |
        | CPU processes
        v
   Audio Queue
        ^
        |
   +---------+
   | Buffer 1|
   +---------+
        ^
        |
        | DMA fills
        |

The two buffers operate in a ping-pong arrangement.

While one buffer is being processed by the CPU, the other buffer can receive new samples.

This prevents the CPU from blocking microphone acquisition.

---

## 5. DMA Buffer Configuration

The reference implementation uses:

- Sample rate: 16 kHz
- Sample width: 16 bits
- Mono audio
- Buffer size: 512 samples
- Two application-side buffers
- FreeRTOS queue for communication between tasks

At 16 kHz, a 512-sample buffer represents approximately:

512 / 16000 = 32 ms

of audio.

This provides a practical balance between:

- Latency
- CPU overhead
- Memory usage
- Network transmission efficiency

The exact buffer size can be tuned based on the target workload.

---

## 6. FreeRTOS Task Separation

The audio capture and network operations are separated.

### Audio Capture Task

Responsibilities:

- Read audio samples from I2S
- Receive completed DMA data
- Switch between buffers
- Push completed audio buffers into a FreeRTOS queue

The capture task does not perform network communication.

### Audio Processing Task

Responsibilities:

- Consume audio buffers
- Perform preprocessing
- Voice activity detection
- Audio compression
- Prepare audio for transmission

### Network Task

The network task is logically separated from audio capture.

Responsibilities:

- Maintain WebSocket connection
- Send image/audio requests
- Receive hub responses
- Handle connection failures and retries

This prevents a slow network operation from blocking microphone acquisition.

---

## 7. Raspberry Pi AI Hub

The Raspberry Pi runs an asynchronous FastAPI application.

The WebSocket endpoint receives:

```json
{
    "request_id": "unique-request-id",
    "image": "<base64-image>",
    "audio": "<base64-audio>"
}

continue with the remaining sections:

```markdown
The server validates the payload and decodes the Base64 data.

It then immediately sends:

```json
{
    "status": "processing",
    "request_id": "unique-request-id",
    "message": "Processing your request..."
}