import cv2
import numpy as np

fgbg = cv2.bgsegm.createBackgroundSubtractorCNT()
cap = cv2.VideoCapture(0)
cap.set(3, 320)
cap.set(4, 240) 

SENSITIVITY = 0.03
LUM_THRESH = 40 
ALPHA = 0.02 # maintain a running avg of 99 frames. A car rolling in will not be flagged as it's gradual-ish (mention this in report & pres)
last_avg_lum = None

def tier1Actions(frame):
    global last_avg_lum
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    curr = np.mean(gray)
    
    if last_avg_lum is None: 
        last_avg_lum = curr
        return False

    triggered = abs(curr - last_avg_lum) > LUM_THRESH
    
    last_avg_lum = (ALPHA * curr) + ((1 - ALPHA) * last_avg_lum)

    mask = fgbg.apply(gray)
    motion = (cv2.countNonZero(mask) / (320*240)) > SENSITIVITY
    
    return triggered or motion

while True:    
    ret, frame = cap.read()
    if not ret or cv2.waitKey(1) & 0xFF == ord('x'): break

    if tier1Actions(frame):
        print("oopsie, will do more!") 