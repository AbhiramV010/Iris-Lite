import cv2
import numpy as np
from captureinfo import * # this has info for main.py
from multiprocessing.connection import Client
import datetime
from collections import deque
from main import definePrivacy

overlap_history = deque(maxlen=10)
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
    fgbg = cv2.createBackgroundSubtractorMOG2(history=200, detectShadows=False)   # Change to CNT if it slows down on the Pi
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

def detectGrassOverlap(grass_mask, px20):
    global is_overlapping, start_time, end_time
    
    small_grass = cv2.resize(grass_mask, (0,0), fx=0.25, fy=0.25, interpolation=cv2.INTER_NEAREST)
    small_px20 = cv2.resize(px20, (0,0), fx=0.25, fy=0.25, interpolation=cv2.INTER_NEAREST)

    overlap = cv2.bitwise_and(small_grass, small_px20)
    current_count = cv2.countNonZero(overlap)
    overlap_history.append(current_count)
    avg_overlap = sum(overlap_history) / len(overlap_history)
    
    if avg_overlap >= 3 and not is_overlapping:
        is_overlapping = True
        start_time = datetime.datetime.now()
        return None
    
    elif avg_overlap < 1 and is_overlapping:
        is_overlapping = False
        end_time = datetime.datetime.now()
        duration = end_time - start_time

        if duration>datetime.timedelta(seconds=2):
            buff_start = (start_time - datetime.timedelta(seconds=2)).strftime("%H:%M:%S")
            buff_end = (end_time + datetime.timedelta(seconds=2)).strftime("%H:%M:%S")
            return CaptureClass(startTime=buff_start, endTime=buff_end, trigger="Grass Overlap", duration=duration.total_seconds())
            
    return None

startCam()    

privacyzone=definePrivacy(cam) 
ret, frame = cam.read()
hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
grassRange = cv2.inRange(hsv, np.array([25, 30, 20]), np.array([95, 255, 255]))
grass_zones = defineZone(grassRange)

grass_mask = np.zeros(frame.shape[:2], dtype=np.uint8)
if grass_zones:
    cv2.drawContours(grass_mask, grass_zones, -1, 255, thickness=-1)

while True:
    cv2.imshow("grass",grass_mask)
    ret, frame = cam.read()
    if not ret:
        break

    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    fgmask = fgbg.apply(frame)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    fgmask = cv2.morphologyEx(fgmask, cv2.MORPH_CLOSE, kernel)
    
    contours, _ = cv2.findContours(fgmask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    contours = sorted(contours,key=cv2.contourArea,reverse=True) # biggest to smallest contours
    contours = contours[:3]
    bottom_mask = np.zeros_like(fgmask)
    
    for idv in contours:
        x, y, w, h = cv2.boundingRect(idv)
        roi_y1 = max(0, y + h - 20)
        roi_y2 = y + h
        actual_object_strip = fgmask[roi_y1:roi_y2, x:x+w]
        bottom_mask[roi_y1:roi_y2, x:x+w] = actual_object_strip

    _, bottom_mask = cv2.threshold(bottom_mask, 127, 255, cv2.THRESH_BINARY)
    cv2.imshow("mm",bottom_mask)
    alert = detectGrassOverlap(grass_mask, bottom_mask)

    if alert:
        try: 
            address = ('127.0.0.1', 8989)
            if (alert.duration) > datetime.timedelta(seconds=2):
                with Client(address, authkey=b'1000011') as conn:
                    conn.send(alert)
        except ConnectionRefusedError: 
            raise ConnectionError("the main.py file may not be running")
        
    cv2.imshow("steamic26-cam", frame)
    if cv2.waitKey(1) & 0xFF == ord('x'):
        break

cam.release()
cv2.destroyAllWindows()