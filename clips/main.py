import cv2
import numpy as np
from multiprocessing.connection import Listener
import threading
from captureinfo import CaptureClass
from collections import deque
import time
import os
import subprocess 

SSD_PATH = "C:/delete/clips" # TODO: SOMEONE needs to add the path where clips will be saved 
BUFFER_MINUTES = 10
FPS = 24  # TODO: make this the actual camera fps
FRAME_BUFFER = deque(maxlen=FPS * 60 * BUFFER_MINUTES) # fps * 60 seconds per min * 10 mins, how many frames to store in RAM
    # The deque object here is very memory UNSAFE! Be careful when editing code

buffer_lock = threading.Lock()

os.makedirs(SSD_PATH, exist_ok=True)

def save_clip_worker(frames_to_save, trigger_name,capture_class: CaptureClass): 
    with buffer_lock:    
        ts = int(time.time()) 
        tmp = f"/dev/shm/t_{ts}"  # save in RAM, because read/write operations can take massive tolls on SSDs, but nothing on RAM
        os.makedirs(tmp, exist_ok=True) 

        for i, f in enumerate(frames_to_save): 
            with open(f"{tmp}/{i:05d}.jpg", "wb") as j: j.write(f) 

        if capture_class.isMotionSensor == True:
            cmd = f"ffmpeg -y -framerate {FPS} -i {tmp}/%05d.jpg -c:v libx264 -preset ultrafast -crf 28 -pix_fmt yuv420p {SSD_PATH}/{trigger_name}_{ts}_mthn.mp4"
        elif capture_class.isDoorSensor == True:
            cmd = f"ffmpeg -y -framerate {FPS} -i {tmp}/%05d.jpg -c:v libx264 -preset ultrafast -crf 28 -pix_fmt yuv420p {SSD_PATH}/{trigger_name}_{ts}_drsn.mp4"
        else: # normal conditions
            cmd = f"ffmpeg -y -framerate {FPS} -i {tmp}/%05d.jpg -c:v libx264 -preset ultrafast -crf 28 -pix_fmt yuv420p {SSD_PATH}/{trigger_name}_{ts}.mp4" 
        subprocess.run(cmd.split(), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) 
        subprocess.run(["rm", "-rf", tmp]) 

def clipRecorder(l):
    while True:
        with l.accept() as conn:
            capture = conn.recv()
            if capture:
                with buffer_lock:
                    buffer_snapshot = list(FRAME_BUFFER)
                threading.Thread(target=save_clip_worker, args=(buffer_snapshot, capture.trigger),daemon=True).start()

if __name__ == "__main__":
    cam = cv2.VideoCapture(0)
    cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
    cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)

    ret, frame = cam.read()
    prev_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    address = ('127.0.0.1', 8989)
    try:
        l = Listener(address, authkey=b'1000011')
        threading.Thread(target=clipRecorder, args=(l,), daemon=True).start()

        while True:
            ret, frame = cam.read()
            if not ret: break

            h, w, _ = frame.shape
            frame[h-300:h, w-400:w] = 0

            # making the privacy zone
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            diff = cv2.absdiff(prev_gray, gray)
            _, motion_mask = cv2.threshold(diff, 25, 255, cv2.THRESH_BINARY)
            motion_mask = cv2.dilate(motion_mask, np.ones((3,3), np.uint8), iterations=1)
            prev_gray = gray

            _, encoded_frame = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 60]) 
            FRAME_BUFFER.append(encoded_frame)

            # cv2.imshow('steamic-c6_cam', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'): break

    finally:
        cam.release()