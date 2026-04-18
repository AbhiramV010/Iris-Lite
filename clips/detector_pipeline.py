import cv2
import numpy as np
from datetime import datetime, timedelta
from captureinfo import CaptureClass
from multiprocessing.connection import Client
from sensor_helper import *

fgbg = cv2.bgsegm.createBackgroundSubtractorCNT()
cap = cv2.VideoCapture(0)
cap.set(3, 320)
cap.set(4, 240) 

SENSITIVITY = 0.10
LUM_THRESH = 90     
ALPHA = 0.05        

global ADDRESS, AUTHKEY
ADDRESS = ('127.0.0.1', 8989)
AUTHKEY = b'1000011'

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
    motion = (cv2.countNonZero(mask) / (320*240)) > SENSITIVITY
    
    return (triggered or motion), gray, mask

for _ in range(0, 150): 
    ret, frame = cap.read()
    if ret: tier1Actions(frame)

try: 
    try: 
        start_up(17) # door sensor
        start_up(27) # motion sensor
    except:
        pass

    while True:    
        ret, frame = cap.read()
        if not ret: break

        vis_frame = frame.copy()
        is_triggered, gray, mask = tier1Actions(frame)

        if is_triggered:
            print("TRIG") # debug
            
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
                    
                    if dist > 10: 
                        persistence_count += 1
                    else:
                        persistence_count = max(0, persistence_count - 1)
                        color = (0, 0, 255) 
                         
                last_centroid = current_centroid
                cv2.circle(vis_frame, current_centroid, 5, color, -1)

                if persistence_count >= 50: 
                    buffered_start = datetime.now() - timedelta(seconds=5)
                    buffered_end = datetime.now() + timedelta(seconds=5)
                    total_duration = (buffered_end - buffered_start).total_seconds()
                    
                    new_capture = CaptureClass(
                        startTime=buffered_start.strftime("%H:%M:%S"), 
                        endTime=buffered_end.strftime("%H:%M:%S"),
                        trigger=f"tiered_cap",
                        duration=round(total_duration, 2),
                        isMotionSensor=check_gpio(17), 
                        isDoorSensor=check_gpio(27)
                    )

                    try:
                        with Client(ADDRESS, authkey=AUTHKEY) as conn:
                            conn.send(new_capture)
                    except:
                        print("the main.py file may not be running")
        else:
            if last_centroid:
                cv2.circle(vis_frame, last_centroid, 5, (0, 0, 255), -1) 
            last_centroid = None
            persistence_count = 0 
         
        cv2.imshow("Centroid Tracker", vis_frame)
        if cv2.waitKey(1) & 0xFF == ord('x'): break

finally:
    try:
        close_gpio(17)
        close_gpio(27)
    except: pass
    cap.release()
    cv2.destroyAllWindows()