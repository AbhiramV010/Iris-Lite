[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](http://creativecommons.org/licenses/by-nc-sa/4.0/)

<p align="center">
  <a href="https://git.io/typing-svg">
    <img src="https://readme-typing-svg.demolab.com?font=Rajdhani&weight=700&size=40&pause=1000&color=7ab8f5&center=true&vCenter=true&width=1000&lines=Iris-Lite;A+spatially-aware+camera+system;Working+to+reduce+storage+usage;and+increase+environmental+friendliness" alt="Iris-Lite Banner" />
  </a>
</p>

<p align="center">
  <a href="https://git.io/typing-svg">
    <img src="https://readme-typing-svg.demolab.com?font=Rajdhani&weight=700&size=40&pause=2500&color=7ab8f5&center=true&vCenter=true&width=1000&lines=STEAM+ICAC+2026;Submission+For+The+Computer+Science+Event;Sustainable,+Private,+Scalable" alt="Event Info" />
  </a>
</p>

**STEAM IC 2026 Computer Science Project**

Project Programming by Abhiram Vadali & Shreyash Thakur

Lab Report by Abhiram V, Shreyash T, Subeg G, and Atharv R.

README.md written by Subeg Singh Gill

---

## Table of Contents

- [Overview](#overview)
- [System Requirements](#system-requirements)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [How It Works](#how-it-works)
- [Compression Pipeline](#compression-pipeline)
- [Troubleshooting](#troubleshooting)
- [License](#license)
- [Acknowledgements](#acknowledgements)

---

## Overview

**Iris_Lite** is a multi-process intelligent camera system built on the **Raspberry Pi 4B**. It captures 1080p video, maintains a rolling 5-minute frame buffer in shared memory, and uses a multi-tiered trigger system — motion detection, audio classification, zone monitoring, and GPIO sensors — to automatically clip and compress relevant events.

When a trigger fires, the C++ compression engine reads directly from the shared frame buffer and encodes the event window to H.264 using hardware acceleration, with per-frame quality dynamically adjusted based on scene importance.

---

## System Requirements

### Hardware

- **Raspberry Pi 4B** (2GB RAM)
- **USB Camera** — MJPG-capable, targeting 1080p via V4L2
- **USB Microphone** — for audio classification
- **GPIO Sensors** — door sensor (pin 17), motion sensor (pin 27)
- **Storage** — microSD or external drive mounted at `/clipDrive/clips/`

### Software

- **OS:** Debian Linux
- **Python:** 3.12.7
- **C++:** GCC 11+ with C++ 20 support (`std::format`)
- **OpenCV:** 4.x with `bgsegm` module (`opencv-contrib-python`)
- **FFmpeg** — with `h264_v4l2m2m` hardware encoder support
- **Other Python packages:** `pyaudio`, `ai-edge-litert`, `RPi.GPIO`, `numpy`

---

## Project Structure

```
Iris-Lite/
├── Compression/
│   ├── include/
│   │   ├── CompressionEngine.hpp
│   │   ├── CompressionPolicy.hpp
│   │   ├── H264Encoder.hpp
│   │   ├── ImportanceEngine.hpp
│   │   ├── SharedFrameBuffer.hpp
│   │   └── Config.hpp
│   ├── src/
│   │   ├── main.cpp              # Entry point, listens for events via shared memory
│   │   ├── CompressionEngine.cpp # Orchestrates frame analysis and encoding
│   │   ├── CompressionPolicy.cpp # Frame drop and CRF decisions
│   │   ├── H264Encoder.cpp       # FFmpeg pipe wrapper
│   │   └── ImportanceEngine.cpp  # Motion, edge, and face scoring
│   └── CMakeLists.txt
│
├── clips/
│   ├── startCamera.py            # V4L2 capture → shared memory
│   ├── main.py                   # Frame buffer manager + event receiver
│   ├── detector_pipeline.py      # Two-tiered motion/luminance detection
│   ├── listener.py               # TFLite audio classification
│   ├── zone_monitor.py           # Grass/zone overlap detection
│   ├── captureinfo.py            # CaptureClass dataclass definition
│   ├── sensor_helper.py          # GPIO abstraction (Linux-only)
│   └── sound_model.tflite        # Trained audio classification model
│
├── log/
│   └── logger.sh
├── IRIS_Lite.sh                  # Main launcher (Bash, NOT zsh)
└── README.md
```

---

## Getting Started

### 1. Install Dependencies

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y ffmpeg libopencv-dev python3-pip cmake build-essential

pip3 install opencv-contrib-python numpy pyaudio ai-edge-litert RPi.GPIO
```

### 2. Clone and Launch

```bash
git clone https://github.com/AbhiramV010/Iris-Lite.git
cd Iris-Lite
bash IRIS_Lite.sh
```

The launcher opens six `uxterm` windows in sequence and monitors startup progress with a loading bar. All six processes must be running for the system to function correctly.

### 3. Storage

Ensure `/clipDrive/clips/` exists and is writable. Clips are saved as timestamped `.mp4` files in the format `iris_lite--YYYY-MM-DD_HH-MM-SS.mp4`.

---

## How It Works

Iris_Lite runs as **six parallel processes**, coordinated through POSIX shared memory and a socket-based event bus on `127.0.0.1:8989`.

**startCamera.py** captures 1080p MJPG frames from the camera via V4L2 and writes them into a named shared memory block (`iris_live_frame`). It also renders a real-time motion diff overlay on a configurable bottom-left zone.

**main.py** reads from that shared memory and maintains a **rolling 5-minute buffer** at 24 FPS, storing JPEG-compressed frames (quality 25) across a circular array of 7,200 slots. It listens on the event bus for incoming `CaptureClass` objects and maps their timestamps to buffer indices for the C++ engine to consume.

**detector_pipeline.py** runs a two-tiered trigger: luminance shift detection and CNT background subtraction. Triggers that exceed an entropy threshold of `3.0` and show centroid movement over 50 consecutive frames send a `CaptureClass` to the event bus, with a ±5 second buffer around the event window.

**listener.py** samples audio at 16kHz, builds mel spectrograms on-device, and runs inference with the bundled TFLite model. It classifies 7 sound categories — screaming, gunshots, glass breaking, aggressive knocking, car screech, dog barking, and ambience — and triggers a capture on any concerning detection.
- The initial H5 file was too large to commit to github, so we compressed it into a TFLite (for storage & Pi purposes) and put that on the repository
- The Deep Learning model was created by us, and it was NOT a pre-trained or pre-created model

**zone_monitor.py** uses GSOC background subtraction and HSV-based grass segmentation to detect when a foreground object overlaps a defined ground zone, triggering a capture after 2+ seconds of sustained overlap.

All triggers include GPIO state from the door sensor (pin 17) and motion sensor (pin 27), recorded in the `CaptureClass` metadata.

---

## Compression Pipeline

The C++ engine (`Compression/`) listens for events via the `iris_concern_indices` shared memory block written by `main.py`. On an event, it:

1. Extracts the relevant frame window from the shared buffer via `SnapshotExtractor`
2. Scores each frame using **ImportanceEngine** — a weighted blend of motion (45%), edge density via Sobel (35%), and face detection via Haar cascade (20%), computed at 320×180
3. Applies **exponential smoothing** (`0.85 * prev + 0.15 * raw`) to prevent erratic quality switches
4. Uses **CompressionPolicy** to decide whether to drop the frame (importance + momentum vs. a `0.55` threshold) and compute a dynamic CRF in the range **16–36**
5. Writes kept frames to FFmpeg via pipe using the **h264_v4l2m2m** hardware encoder on Linux, falling back to `libx264 -preset ultrafast` otherwise

CRF segments are only re-opened when the computed CRF shifts by ≥5 from the current value, reducing segment fragmentation.

---

## Troubleshooting

**Camera not detected**
Verify the device is accessible via `/dev/video0` and supports MJPG at 1920×1080:
```bash
v4l2-ctl --list-formats-ext
```

**Shared memory error on startup**
If `startCamera.py` crashes, stale shared memory blocks may persist. Clear them:
```bash
python3 -c "from multiprocessing import shared_memory; shared_memory.SharedMemory('iris_live_frame').unlink()"
```

**Compression engine not encoding**
Ensure `/clipDrive/clips/` exists and FFmpeg supports the hardware encoder:
```bash
ffmpeg -codecs | grep h264_v4l2m2m
```

**GPIO errors on non-Linux systems**
`sensor_helper.py` automatically disables GPIO on non-Linux platforms. This is expected behavior during development on Windows or macOS.

**Audio device not found**
`listener.py` scans for USB or hardware audio devices first. If none match, it falls back to the system default. List available devices with `arecord -l`.

---

## License

Licensed under **CC BY-NC-SA 4.0** — [creativecommons.org/licenses/by-nc-sa/4.0](http://creativecommons.org/licenses/by-nc-sa/4.0/)

Any modifications or derivatives **must** be redistributed under the same license with full attribution.

---
### Lab Report
<iframe src="https://1drv.ms/b/c/d8ce14bd3ad1bc1f/IQBHyXCz5A8VR6vFVnotKBCTAYs_GCPcj3222Oq34QaS6MQ?e=aXhtK3" width="100%" height="500px"></iframe>

---

## Acknowledgements

- **Mr. Hadley** — For his support throughout development
- **Macmeet B** — For lending the Raspberry Pi 4B that made this project possible
