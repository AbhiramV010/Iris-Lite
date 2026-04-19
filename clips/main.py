import cv2
import numpy as np
from multiprocessing.connection import Listener
import threading
from captureinfo import CaptureClass
from collections import deque
import time
import os
import subprocess
import shutil
import ctypes
import warnings

SSD_PATH = "clipDrive/clips" # TODO: SOMEONE needs to add the path where clips will be saved 
BUFFER_MINUTES = 5 
FPS = 24  # TODO: make this the actual camera fps
FRAME_BUFFER = deque(maxlen=FPS * 60 * BUFFER_MINUTES) # fps * 60 seconds per min * 10 mins, how many frames to store in RAM
MCL_CURRENT = 1
MCL_FUTURE = 2

def lock_memory():
    try:
        ctypes.CDLL("libc.so.6").mlockall(MCL_CURRENT | MCL_FUTURE)
    except Exception as e:
        warnings.warn("Memory-locking failed. Unexpected things could happen.", RuntimeWarning)

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
    elif capture_class.isDoorSensor == True:
        cmd = f"ffmpeg -y -framerate {FPS} -i {tmp}/%05d.jpg -c:v libx264 -preset ultrafast -crf 28 -pix_fmt yuv420p {SSD_PATH}/{trigger_name}_{ts}_drsn.mp4"
    else: # normal conditions
        cmd = f"ffmpeg -y -framerate {FPS} -i {tmp}/%05d.jpg -c:v libx264 -preset ultrafast -crf 28 -pix_fmt yuv420p {SSD_PATH}/{trigger_name}_{ts}.mp4" 
    
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
    cam = cv2.VideoCapture(0, cv2.CAP_V4L2)
    cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
    cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)
    cam.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    ret, frame = cam.read()
    small_prev = cv2.resize(cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY), (480, 270))

    address = ('127.0.0.1', 8989)
    try:
        l = Listener(address, authkey=b'1000011')
        threading.Thread(target=clipRecorder, args=(l,), daemon=True).start()

        while True:
            ret, frame = cam.read()
            if not ret: break

            h, w, _ = frame.shape
            frame[h-450:h, w-600:w] = 0

            # making the privacy zone
            small_gray = cv2.resize(cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY), (480, 270))
            diff = cv2.absdiff(small_prev, small_gray)
            _, motion_mask = cv2.threshold(diff, 25, 255, cv2.THRESH_BINARY)
            motion_mask = cv2.dilate(motion_mask, np.ones((3,3), np.uint8), iterations=1)
            small_prev = small_gray

            _, encoded_frame = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 40])
            with buffer_lock:
                FRAME_BUFFER.append(encoded_frame)

            cv2.imshow('irisLiteCam', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'): break

    finally:
        cam.release()
        cv2.destroyAllWindows()
