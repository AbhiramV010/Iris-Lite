import cv2
import numpy as np
from captureinfo import * 
from multiprocessing.connection import Client
from multiprocessing import shared_memory
import datetime
from collections import deque
from sensor_helper import *
import sys

W, H = 1920, 1080
SHM_NAME = "iris_live_frame" # pull from the shm

overlap_history = deque(maxlen=10)
is_overlapping = False
start_time = None
end_time = None

def getPrefConts(cnts: list):
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
    centroids = centroids[1:6]
    for c in centroids:
        d = np.sqrt((bigCent[0]-c[0])**2+(bigCent[1]-c[1])**2)
        dists.append(d)
    try: return tuple((0,dists.index(min(dists))))
    except: return []

def defineZone(mask):
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (4, 4))
    mask=cv2.dilate(mask,kernel,iterations=2)
    mask=cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
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
            buff_start = (start_time - datetime.timedelta(seconds=5)).strftime("%H:%M:%S")
            buff_end = (end_time + datetime.timedelta(seconds=5)).strftime("%H:%M:%S")
            return CaptureClass(startTime=buff_start, endTime=buff_end, trigger="Grass Overlap", duration=duration, isMotionSensor=check_gpio(17), isDoorSensor=check_gpio(27))
    return None

shm = shared_memory.SharedMemory(name=SHM_NAME)
shared_frame = np.ndarray((H, W, 3), dtype=np.uint8, buffer=shm.buf)

fgbg = cv2.bgsegm.createBackgroundSubtractorGSOC(nSamples=20, replaceRate=0.003, 
                                                 propagationRate=0.01, hitsThreshold=32)

frame_raw = shared_frame.copy()
frame = cv2.resize(frame_raw, (640, 360))
hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
grassRange = cv2.inRange(hsv, np.array([25, 30, 20]), np.array([95, 255, 255]))
grass_zones = defineZone(grassRange)
grass_mask = np.zeros(frame.shape[:2], dtype=np.uint8)
if grass_zones:
    cv2.drawContours(grass_mask, grass_zones, -1, 255, thickness=-1)

try:
    try: 
        start_up(17)
        start_up(27)
    except: pass
    while True:
        frame_raw = shared_frame.copy()
        frame = cv2.resize(frame_raw, (640, 360))
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        fgmask = fgbg.apply(frame)
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        fgmask = cv2.morphologyEx(fgmask, cv2.MORPH_CLOSE, kernel)
        contours, _ = cv2.findContours(fgmask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        contours = sorted(contours,key=cv2.contourArea,reverse=True)[:3]
        bottom_mask = np.zeros_like(fgmask)
        for idv in contours:
            x, y, w, h = cv2.boundingRect(idv)
            roi_y1, roi_y2 = max(0, y + h - 20), y + h
            bottom_mask[roi_y1:roi_y2, x:x+w] = fgmask[roi_y1:roi_y2, x:x+w]
        _, bottom_mask = cv2.threshold(bottom_mask, 127, 255, cv2.THRESH_BINARY)
        combined_view = cv2.addWeighted(grass_mask, 0.5, bottom_mask, 1.0, 0)
        # cv2.imshow("zone_monitor", combined_view)

        # cv2.imshow("raw", frame_raw) # TODO: remove ts
        alert = detectGrassOverlap(grass_mask, bottom_mask)
        if alert:
            try: 
                address = ('127.0.0.1', 8989)
                with Client(address, authkey=b'1000011') as conn:
                    conn.send(alert)
            except: pass
        if cv2.waitKey(1) & 0xFF == ord('x'): break
finally:
    try:
        close_gpio(17)
        close_gpio(27)
    except: pass
    shm.close()
    cv2.destroyAllWindows()