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
import sys

SSD_PATH = "clipDrive/clips" 
BUFFER_MINUTES = 5 
FPS = 24  
FRAME_BUFFER = deque(maxlen=FPS * 60 * BUFFER_MINUTES) 
MCL_CURRENT = 1
MCL_FUTURE = 2

# History for reactive mask
ZONE_HISTORY = deque(maxlen=5)
# Privacy zone relative to 1080p: Bottom-right 600x450
ZONE_X, ZONE_Y, ZONE_W, ZONE_H = 1320, 630, 600, 450

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
    else: 
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
    cam = cv2.VideoCapture(0)
    if not cam.isOpened():
        cam = cv2.VideoCapture(0, cv2.CAP_V4L2)
    
    if not cam.isOpened():
        sys.exit(1)

    cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
    cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)
    cam.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    time.sleep(1.0)
    
    frame = None
    for _ in range(10):
        ret, frame = cam.read()
        if ret and frame is not None:
            break
        time.sleep(0.1)

    if frame is None:
        cam.release()
        sys.exit(1)

    small_prev = cv2.resize(cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY), (480, 270))

    address = ('127.0.0.1', 8989)
    try:
        l = Listener(address, authkey=b'1000011')
        threading.Thread(target=clipRecorder, args=(l,), daemon=True).start()

        while True:
            ret, frame = cam.read()
            if not ret or frame is None: continue

            # process the privacy zone
            try:
                roi = frame[ZONE_Y:ZONE_Y+ZONE_H, ZONE_X:ZONE_X+ZONE_W] 
                roi_gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)

                if len(ZONE_HISTORY) == 5:
                    hist_avg = np.mean(list(ZONE_HISTORY), axis=0).astype(np.uint8)
                    t_diff = cv2.absdiff(roi_gray, hist_avg)
                    _, r_mask = cv2.threshold(t_diff, 25, 255, cv2.THRESH_BINARY)
                    r_mask = cv2.dilate(r_mask, np.ones((3,3), np.uint8), iterations=1)
                    
                    abstract_roi = cv2.bitwise_and(roi, roi, mask=r_mask)
                    frame[ZONE_Y:ZONE_Y+ZONE_H, ZONE_X:ZONE_X+ZONE_W] = 0
                    frame[ZONE_Y:ZONE_Y+ZONE_H, ZONE_X:ZONE_X+ZONE_W] = abstract_roi
                else:
                    frame[ZONE_Y:ZONE_Y+ZONE_H, ZONE_X:ZONE_X+ZONE_W] = 0

                ZONE_HISTORY.append(roi_gray)
            except:
                pass

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