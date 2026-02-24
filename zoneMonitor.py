import cv2
import numpy as np

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
    fgbg = cv2.bgsegm.createBackgroundSubtractorCNT()
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

startCam()

ret, frame = cam.read()
hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
grassRange = cv2.inRange(hsv, np.array([25, 30, 20]), np.array([95, 255, 255]))
grass_zones = defineZone(grassRange)

grass_mask = np.zeros(frame.shape[:2], dtype=np.uint8)
if grass_zones:
    cv2.drawContours(grass_mask, grass_zones, -1, 255, thickness=-1)

cv2.imshow("grassMask",grass_mask) #debug

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
    
    for idv in contours:
        x, y, w, h = cv2.boundingRect(idv)
        cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)

        roi_y1 = y+h-10
        roi_y2 = y+h
        roi_x1 = x
        roi_x2 = x+w

        if roi_y1 >= 0 and roi_y1 < roi_y2 and roi_x1 < roi_x2:
            objectBase = fgmask[roi_y1:roi_y2, roi_x1:roi_x2]
            cv2.imshow("Bottom Section", objectBase)   

    ## perframe ends here

    cv2.imshow("the dot",objectBase) # debug
    cv2.imshow("steamic26-cam", frame)

    if cv2.waitKey(1) & 0xFF == ord('x'):
        break

cv2.destroyAllWindows()