import cv2
import numpy as np
from captureinfo import * # this has info for main.py
from multiprocessing.connection import Client
from datetime import datetime
from collections import deque
from main import definePrivacy

overlap_history = deque(maxlen=10)
dest = ("127.0.0.1",6000)
is_overlapping = False
start_time = None
end_time = None

def getPrefConts(cnts: list): # get contours that correspond to potential grass(es)
    centroids = []
    cnts = sorted(cnts, key=cv2.contourArea, reverse=True)
    dists = []
    for c in cnts:
        M = cv2.moments(c)
        if M["m00"] != 0:
            cx = int(M["m10"] / M["m00"])
            cy = int(M["m01"] / M["m00"])
            centroids.append((cx, cy))
        else:
            centroids.append((0, 0))

    bigCent = centroids[0]
    centroids = centroids[1:6] # top 6 largest contours (excl first one)
    
    for c in centroids:
        d = np.sqrt((bigCent[0]-c[0])**2+(bigCent[1]-c[1])**2)
        dists.append(d)

    try: return tuple((0,dists.index(min(dists))))
    except: return []

def startCam():
    global cam, ret, frame, hsv, grassRange, fgbg
    fgbg = cv2.createBackgroundSubtractorKNN(history=500,dist2Threshold=800.0,detectShadows=True)   # Change to CNT if it slows down on the Pi
    cam = cv2.VideoCapture(0)
    for _ in range(0,120):cam.read()

def defineZone(mask):
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (4, 4))

    mask=cv2.dilate(mask,kernel,iterations=2)
    mask=cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel) # NOTE: do not make the kernel bigger, use iteration with the same kernel 
    mask=cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)

    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours: return []
    contours = sorted(contours, key=cv2.contourArea, reverse=True)
    contourIndex=getPrefConts(contours)
    
    try: return [contours[contourIndex[0]],contours[contourIndex[1]]]
    except: return []

def finalActions(grass_mask, px20):
    global is_overlapping, start_time, end_time
    overlap = cv2.bitwise_and(grass_mask, px20)
    overlap_history.append(cv2.countNonZero(overlap))
    overlap_count = sum(overlap_history) / len(overlap_history)
    
    if overlap_count >= 50 and not is_overlapping:
        is_overlapping = True
        start_time = datetime.now().strftime("%H:%M:%S")
        start_time = datetime.strptime(start_time, "%H:%M:%S")

        end_time = None 
    
    elif overlap_count < 50 and is_overlapping:
        is_overlapping = False
        end_time = datetime.now().strftime("%H:%M:%S")
        end_time = datetime.strptime(end_time, "%H:%M:%S")

        res = CaptureClass(start_time, end_time, "Grass Detector",duration=end_time-start_time)
        
        start_time = None
        end_time = None
        return res

    return None

startCam()    

print("You will be drawing a privacy zone, blocking out any property that isn't yours. Press 'M' to exit the window")
privacyzone=definePrivacy(cam) 

ret, frame = cam.read()
hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
grassRange = cv2.inRange(hsv, np.array([25, 30, 20]), np.array([95, 255, 255]))
grass_zones = defineZone(grassRange)

grass_mask = np.zeros(frame.shape[:2], dtype=np.uint8)
if grass_zones:
    cv2.drawContours(grass_mask, grass_zones, -1, 255, thickness=-1)

while True:
    ret, frame = cam.read()
    if not ret:
        break
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    
    fgmask=fgbg.apply(frame)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    fgmask = cv2.morphologyEx(fgmask, cv2.MORPH_CLOSE, kernel)
    
    contours, _ = cv2.findContours(fgmask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    contours = sorted(contours,key=cv2.contourArea,reverse=True) # biggest to smallest contours
    contours = contours[:3]
    bottom_only_mask = np.zeros_like(fgmask)
    
    for idv in contours:
        x, y, w, h = cv2.boundingRect(idv)
        roi_y1 = max(0, y + h - 20)
        roi_y2 = y + h
        actual_object_strip = fgmask[roi_y1:roi_y2, x:x+w]
        bottom_only_mask[roi_y1:roi_y2, x:x+w] = actual_object_strip

    
    try: frame[privacyzone[0][1]:privacyzone[1][1], privacyzone[0][0]:privacyzone[1][0]] = 0 # black out priv zone, remove when done cause the C++ needs to do this
    except: pass

    _, bottom_only_mask = cv2.threshold(bottom_only_mask, 127, 255, cv2.THRESH_BINARY)
    alert = finalActions(grass_mask, bottom_only_mask)

    if alert:
        try: 
            address = ('127.0.0.1', 8989)
            if (alert.endTime - alert.startTime) > datetime.timedelta(seconds=2):
                with Client(address, authkey=b'1000011') as conn:
                    conn.send(alert)

        except ConnectionRefusedError: 
            raise ConnectionError("the main.py file may not be running")
        except: raise RuntimeError("Something else went wrong")

    cv2.imshow("steamic26-cam", frame)

    if cv2.waitKey(1) & 0xFF == ord('x'):
        break

cv2.destroyAllWindows()