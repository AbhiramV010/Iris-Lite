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

SSD_PATH = "clipDrive/clips" 
BUFFER_MINUTES = 5 
FPS = 24  
FRAME_BUFFER = deque(maxlen=FPS * 60 * BUFFER_MINUTES) 
MCL_CURRENT = 1
MCL_FUTURE = 2
SHM_NAME = "iris_live_frame"

def lock_memory():
    try:
        ctypes.CDLL("libc.so.6").mlockall(MCL_CURRENT | MCL_FUTURE)
    except Exception as e:
        warnings.warn("Failed to lock memory.", RuntimeWarning)

buffer_lock = threading.Lock()

os.makedirs(SSD_PATH, exist_ok=True)

def save_clip_worker(trigger_name, capture_class: CaptureClass): 
    ts = int(time.time()) 
    tmp = f"/tmp/t_{ts}"
    os.makedirs(tmp, exist_ok=True) 

    with buffer_lock:
        for i, f in enumerate(FRAME_BUFFER): 
            with open(f"{tmp}/{i:05d}.jpg", "wb") as j: 
                j.write(f) 
    if capture_class.isMotionSensor == True:
        cmd = f"ffmpeg -y -framerate {FPS} -i {tmp}/%05d.jpg -c:v libx264 -preset ultrafast -crf 28 -pix_fmt yuv420p {SSD_PATH}/{trigger_name}_{ts}_mthn.mp4"
        print(f"start: {capture_class.startTime} | end: {capture_class.endTime}")
    elif capture_class.isDoorSensor == True:
        cmd = f"ffmpeg -y -framerate {FPS} -i {tmp}/%05d.jpg -c:v libx264 -preset ultrafast -crf 28 -pix_fmt yuv420p {SSD_PATH}/{trigger_name}_{ts}_drsn.mp4"
        print(f"start: {capture_class.startTime} | end: {capture_class.endTime}")
    else: 
        cmd = f"ffmpeg -y -framerate {FPS} -i {tmp}/%05d.jpg -c:v libx264 -preset ultrafast -crf 28 -pix_fmt yuv420p {SSD_PATH}/{trigger_name}_{ts}.mp4"
        print(f"start: {capture_class.startTime} | end: {capture_class.endTime}")
    
    subprocess.run(cmd.split(), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) 
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

            # Limit to 24 FPS to save CPU
            elapsed = time.time() - t_start
            time.sleep(max(1/FPS - elapsed, 0.001))

    except KeyboardInterrupt:
        pass
    finally:
        shm.close()