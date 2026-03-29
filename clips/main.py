import cv2
import numpy as np
from multiprocessing.connection import Listener
import threading
from captureinfo import CaptureClass
from collections import deque
import time
import os
import subprocess 

SSD_PATH = "" # TODO: Shreyash needs to add the path where clips will be saved 
BUFFER_MINUTES = 10
FPS = 24  # TODO: make this the actual camera fps
FRAME_BUFFER = deque(maxlen=FPS * 60 * BUFFER_MINUTES)

buffer_lock = threading.Lock()

os.makedirs(SSD_PATH, exist_ok=True)

start_point = None
end_point = None
drawing = False

def drawRectangle(event, x, y, flags, param):
    global start_point, end_point, drawing
    if event == cv2.EVENT_LBUTTONDOWN:
        start_point, end_point, drawing = (x, y), (x, y), True
    elif event == cv2.EVENT_MOUSEMOVE and drawing:
        end_point = (x, y)
    elif event == cv2.EVENT_LBUTTONUP:
        drawing, end_point = False, (x, y)

def save_clip_worker(frames_to_save, trigger_name):
    with buffer_lock:    
        ts = int(time.time()) 
        tmp = f"/dev/shm/t_{ts}"  # save in RAM, because read/write operations can take massive tolls on SSDs, but nothing on RAM
        os.makedirs(tmp, exist_ok=True) 

        for i, f in enumerate(frames_to_save): 
            with open(f"{tmp}/{i:05d}.jpg", "wb") as j: j.write(f) 

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
    
    cv2.namedWindow('define_privacy_zone')
    cv2.setMouseCallback('define_privacy_zone', drawRectangle)

    while True:
        ret, frame = cam.read()
        if not ret: break
        if start_point and end_point:
            cv2.rectangle(frame, start_point, end_point, (0, 255, 0), 2)
        cv2.imshow('define_privacy_zone', frame)
        if cv2.waitKey(1) & 0xFF == ord('m'): 
            break
    cv2.destroyWindow('define_privacy_zone')

    ret, frame = cam.read()
    prev_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    address = ('127.0.0.1', 8989)
    try:
        l = Listener(address, authkey=b'1000011')
        threading.Thread(target=clipRecorder, args=(l,), daemon=True).start()
        
        while True:
            ret, frame = cam.read()
            if not ret: break

            # making the privacy zone
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            diff = cv2.absdiff(prev_gray, gray)
            _, motion_mask = cv2.threshold(diff, 25, 255, cv2.THRESH_BINARY)
            motion_mask = cv2.dilate(motion_mask, np.ones((3,3), np.uint8), iterations=1)
            prev_gray = gray

            if start_point and end_point:
                x1, y1 = min(start_point[0], end_point[0]), min(start_point[1], end_point[1])
                x2, y2 = max(start_point[0], end_point[0]), max(start_point[1], end_point[1])
                if x2 > x1 and y2 > y1:
                    roi = frame[y1:y2, x1:x2]
                    mask_roi = motion_mask[y1:y2, x1:x2]
                    frame[y1:y2, x1:x2] = cv2.bitwise_and(roi, roi, mask=mask_roi)

            _, encoded_frame = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 60]) 
            FRAME_BUFFER.append(encoded_frame)

            cv2.imshow('steamic-c6_cam', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'): break

    finally:
        cam.release()
        cv2.destroyAllWindows()