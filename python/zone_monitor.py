import cv2
import numpy as np
from captureinfo import * # this has info that will be sent to main.py
from multiprocessing.connection import Client
import datetime
from collections import deque
from main import definePrivacy

# This script will process in 360p, and the rec in main.py will be in 1080p
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
    global cam, fgbg
    fgbg = cv2.createBackgroundSubtractorMOG2(history=200, detectShadows=False)
    cam = cv2.VideoCapture(0)
    cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
    cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)
    for _ in range(0, 60): cam.read()

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
    
    overlap = cv2.bitwise_and(grass_mask, px20)
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
        duration = (end_time - start_time).total_seconds()

        if duration > 2:
            buff_start = (start_time - datetime.timedelta(seconds=2)).strftime("%H:%M:%S")
            buff_end = (end_time + datetime.timedelta(seconds=2)).strftime("%H:%M:%S")
            return CaptureClass(startTime=buff_start, endTime=buff_end, trigger="Grass Overlap", duration=duration)
            
    return None
startCam()    

privacyzone=definePrivacy(cam) 
if privacyzone: # quick convert 1080 coords from definePrivacy to 360p
    p1, p2 = privacyzone
    p1 = (int(p1[0] * (640/1920)), int(p1[1] * (360/1080)))
    p2 = (int(p2[0] * (640/1920)), int(p2[1] * (360/1080)))
    privacyzone = (p1, p2)
ret, frame_raw = cam.read()
frame = cv2.resize(frame_raw, (640, 360))
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
    frame = cv2.resize(frame_raw, (640, 360))

    if privacyzone:
        mask = np.zeros(frame.shape[:2], dtype=np.uint8)
        
        p1, p2 = privacyzone
        rect_pts = np.array([
            [p1[0], p1[1]], [p2[0], p1[1]], 
            [p2[0], p2[1]], [p1[0], p2[1]]
        ], dtype=np.int32)

        cv2.drawContours(mask, [rect_pts], -1, 255, thickness=-1)
        
        x, y, w, h = cv2.boundingRect(mask)
        if privacyzone:
            cv2.rectangle(frame, privacyzone[0], privacyzone[1], (0, 0, 0), -1) 

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
            if alert.duration > 2.0:
                with Client(address, authkey=b'1000011') as conn:
                    conn.send(alert)
        except ConnectionRefusedError: 
            raise ConnectionError("the main.py file may not be running")
        
    cv2.imshow("steamic26-cam", frame)
    if cv2.waitKey(1) & 0xFF == ord('x'):
        break

cam.release()
cv2.destroyAllWindows()