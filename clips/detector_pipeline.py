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
last_avg_lum = None

SENSITIVITY = 0.03
LUM_THRESH = 40 
ALPHA = 0.05 # keep a running average of the last 39 frames.

global ADDRESS, AUTHKEY
ADDRESS = ('127.0.0.1', 8989)
AUTHKEY = b'1000011'

def tier1Actions(frame):
    global last_avg_lum
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    curr = np.mean(gray)
    
    if last_avg_lum is None: 
        last_avg_lum = curr
        return False

    triggered = abs(curr - last_avg_lum) > LUM_THRESH
    last_avg_lum = (ALPHA * curr) + ((1 - ALPHA) * last_avg_lum)

    global mask
    mask = fgbg.apply(gray)
    motion = (cv2.countNonZero(mask) / (320*240)) > SENSITIVITY
    
    return triggered or motion

for _ in range (0,150): 
    ret,frame = cap.read()
    if ret: tier1Actions(frame)

last_centroid = None
persistence_count = 0

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
        is_triggered = tier1Actions(frame)
         

        if is_triggered:
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            if contours:
                main_obj = max(contours, key=cv2.contourArea)
                x, y, w, h = cv2.boundingRect(main_obj)
                current_centroid = (x + w//2, y + h//2)

                 
                # Default color is Green when active, shifts to Red if logic fails below
                color = (0, 255, 0) 
                 

                if last_centroid:
                    dist = np.sqrt((current_centroid[0]-last_centroid[0])**2 + (current_centroid[1]-last_centroid[1])**2)
                    
                    if dist > 10: 
                        persistence_count += 1
                    else:
                        persistence_count = max(0, persistence_count - 1)
                         
                        color = (0, 0, 255) # Red if stagnant
                         
                
                last_centroid = current_centroid

                 
                cv2.circle(vis_frame, current_centroid, 5, color, -1)
                 

                if persistence_count >= 20:
                    buffered_start = datetime.now() - timedelta(seconds=5)
                    buffered_end = datetime.now() + timedelta(seconds=5)
                    total_duration = (buffered_end - buffered_start).total_seconds()
                    
                    new_capture = CaptureClass(startTime=buffered_start.strftime("%H:%M:%S"), endTime=buffered_end.strftime("%H:%M:%S"),trigger=f"tiered_cap",duration=round(total_duration, 2),isMotionSensor=check_gpio(17), isDoorSensor=check_gpio(27))

                    try:
                        with Client(ADDRESS, authkey=AUTHKEY) as conn:
                            conn.send(new_capture)
                    except:
                        print("the main.py file may not be running")
        else:
             
            if last_centroid:
                cv2.circle(vis_frame, last_centroid, 5, (0, 0, 255), -1) # Red because entropy/motion not triggered
             
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