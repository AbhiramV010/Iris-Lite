import cv2
import numpy as np
from multiprocessing.connection import Listener
from multiprocessing import shared_memory
import threading
from captureinfo import CaptureClass
from collections import deque
import time
import os
import subprocess
import shutil
import ctypes
import warnings
import sys

SSD_PATH = "/mnt/clipDrive/clips"
BUFFER_MINUTES = 5 
FPS = 24  
FRAME_BUFFER = deque(maxlen=FPS * 60 * BUFFER_MINUTES) 
MCL_CURRENT = 1
MCL_FUTURE = 2
SHM_NAME = "iris_live_frame"
AUDIO_TMP = "/dev/shm/live_audio.aac"

def lock_memory():
    try:
        ctypes.CDLL("libc.so.6").mlockall(MCL_CURRENT | MCL_FUTURE)
    except Exception as e:
        warnings.warn("Failed to lock memory.", RuntimeWarning)

buffer_lock = threading.Lock()

os.makedirs(SSD_PATH, exist_ok=True)

audio_proc = subprocess.Popen([
    "ffmpeg", "-y", "-f", "alsa", "-ac", "1", "-i", "default", 
    "-c:a", "aac", "-b:a", "48k", AUDIO_TMP
], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def save_clip_worker(trigger_name, capture_class: CaptureClass): 
    ts = int(time.time()) 
    tmp = f"/dev/shm/t_{ts}"
    os.makedirs(tmp, exist_ok=True) 

    with buffer_lock:
        for i, f in enumerate(FRAME_BUFFER): 
            with open(f"{tmp}/{i:05d}.jpg", "wb") as j: 
                j.write(f) 
    
    suffix = "mthn" if capture_class.isMotionSensor else "drsn" if capture_class.isDoorSensor else ""
    label = f"_{suffix}" if suffix else ""
    out_path = f"{SSD_PATH}/{trigger_name}_{ts}{label}.mp4"

    cmd = [
        "ffmpeg", "-y", "-framerate", str(FPS), "-i", f"{tmp}/%05d.jpg",
        "-i", AUDIO_TMP, "-c:v", "h264_v4l2m2m", "-b:v", "2M",
        "-c:a", "copy", "-map", "0:v:0", "-map", "1:a:0", 
        "-shortest", out_path
    ]
    
    print(f"start: {capture_class.startTime} | end: {capture_class.endTime} | reason: {capture_class.trigger}")
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) 
    shutil.rmtree(tmp) 

def clipRecorder(l):
    while True:
        try:
            with l.accept() as conn:
                capture = conn.recv()
                if capture:
                    threading.Thread(target=save_clip_worker, args=(capture.trigger, capture), daemon=True).start()
        except: continue

if __name__ == "__main__":
    lock_memory()

    try:
        shm = shared_memory.SharedMemory(name=SHM_NAME)
        stream_view = np.ndarray((1080, 1920, 3), dtype=np.uint8, buffer=shm.buf)
    except FileNotFoundError:
        sys.exit(1)

    address = ('127.0.0.1', 8989)
    try:
        l = Listener(address, authkey=b'1000011')
        threading.Thread(target=clipRecorder, args=(l,), daemon=True).start()

        while True:
            t_start = time.time()

            frame = stream_view.copy()

            _, encoded_frame = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 15]) 
            with buffer_lock:
                FRAME_BUFFER.append(encoded_frame)

            elapsed = time.time() - t_start
            time.sleep(max(1/FPS - elapsed, 0.001))

    except KeyboardInterrupt:
        pass
    finally:
        audio_proc.terminate()
        shm.close()