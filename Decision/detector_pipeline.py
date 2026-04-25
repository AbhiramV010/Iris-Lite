import cv2
import numpy as np
from datetime import datetime, timedelta
from captureinfo import CaptureClass
from multiprocessing.connection import Client
from multiprocessing import shared_memory
from sensor_helper import *
import sys

W, H = 1920, 1080
SHM_NAME = "iris_live_frame" # pull from the shm

fgbg = cv2.bgsegm.createBackgroundSubtractorCNT()
try:
    shm = shared_memory.SharedMemory(name=SHM_NAME)
    shared_frame = np.ndarray((H, W, 3), dtype=np.uint8, buffer=shm.buf)
except FileNotFoundError:
    raise OSError("Camera not plugged in, OR main.py & startCamera.py aren't running")

SENSITIVITY = 0.25
LUM_THRESH = 90     
ALPHA = 0.05        

ADDRESS = ('127.0.0.1', 8989)
AUTHKEY = b'1000011'

global persistence_count, last_centroid # keep the global here

last_avg_lum = None
persistence_count = 0
last_centroid = None

def calculate_entropy(roi):
    hist = cv2.calcHist([roi], [0], None, [256], [0, 256])
    hist /= (hist.sum() + 1e-7)
    hist = hist[hist > 0]
    return -np.sum(hist * np.log2(hist))

def tier1Actions(frame):
    global last_avg_lum
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    curr = np.mean(gray)
    
    if last_avg_lum is None: 
        last_avg_lum = curr
        return False, gray, None

    triggered = abs(curr - last_avg_lum) > LUM_THRESH
    last_avg_lum = (ALPHA * curr) + ((1 - ALPHA) * last_avg_lum)

    mask = fgbg.apply(gray)
    motion = (cv2.countNonZero(mask) / (640*480)) > SENSITIVITY
    
    return (triggered or motion), gray, mask

try: 
    try: 
        start_up(17) # door sensor
        start_up(27) # motion sensor
    except:
        pass

    while True:   
        vis = shared_frame.copy() # safety copy
        is_triggered, gray, mask = tier1Actions(shared_frame) 

        if is_triggered:
            roi = gray[mask > 0] if np.any(mask) else np.array([])
            entropy_val = calculate_entropy(roi) if roi.size > 0 else 0
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            if entropy_val > 3.0 and contours:
                main_obj = max(contours, key=cv2.contourArea)
                x, y, w, h = cv2.boundingRect(main_obj)
                current_centroid = (x + w//2, y + h//2)
                color = (0, 255, 0) 
                 
                if last_centroid:
                    dist = np.sqrt((current_centroid[0]-last_centroid[0])**2 + (current_centroid[1]-last_centroid[1])**2)
                    if dist > 20: 
                        persistence_count += 1
                    else:
                        persistence_count = max(0, persistence_count - 1)
                        color = (0, 0, 255) 
                         
                last_centroid = current_centroid
                cv2.circle(vis, current_centroid, 10, color, -1)

                if persistence_count >= 50: 
                    new_capture = CaptureClass(
                        startTime=(datetime.now() - timedelta(seconds=5)).strftime("%H:%M:%S"), 
                        endTime=(datetime.now() + timedelta(seconds=5)).strftime("%H:%M:%S"),
                        trigger=f"tiered_cap", duration=10.0, isMotionSensor=check_gpio(27), isDoorSensor=check_gpio(17))
                    try:
                        with Client(ADDRESS, authkey=AUTHKEY) as conn:
                            conn.send(new_capture)
                        persistence_count = 0 # reset perst ct
                    except:
                        raise ConnectionRefusedError("The sending of CaptureClass failed")
        else:
            if last_centroid:
                cv2.circle(vis, last_centroid, 10, (0, 0, 255), -1) 
            last_centroid = None
            persistence_count = 0 
         
        cv2.imshow("Two-tiered detection", vis)
        if cv2.waitKey(1) & 0xFF == ord('x'): break
finally:
    try:
        close_gpio(17)
        close_gpio(27)
    except: pass
    shm.close()
    cv2.destroyAllWindows()