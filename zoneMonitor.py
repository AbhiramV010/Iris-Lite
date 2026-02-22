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

    return tuple((0,dists.index(min(dists))))

def startCam():
    global cam, ret, frame, hsv, grassRange, fgbg
    bgSep = cv2.bgsegm.createBackgroundSubtractorCNT()
    frame = cv2.imread("C:\\Users\\abhir\\Downloads\\STEAMIC_TESTOG3.png")

    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    grassRange = cv2.inRange(hsv, np.array([25, 40, 20]), np.array([95, 255, 255]))

def defineZone(mask):
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (4, 4))

    mask=cv2.dilate(mask,kernel,iterations=2)
    mask=cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel) # NOTE: do not make the kernel bigger, use iteration with the same kernel 
    mask=cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)

    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours: return []
    contours = sorted(contours, key=cv2.contourArea, reverse=True)
    contourIndex=getPrefConts(contours)
    
    return [contours[contourIndex[0]],contours[contourIndex[1]]]

def perFrameGrass(): 
    # TODO: 
    # check the presence of an moving object on the grass
        # bottom-most part of moving object MUST be on the grass, if not just disregard
    # needs to be moving because people run on grass, bike bikes on grass, but a stationary tree doesn't need to be counted, ts supposed to be there
    pass 

startCam()
grass_zones = defineZone(grassRange)
frame_display = frame.copy()

if grass_zones: 
    overlay = frame_display.copy()
    cv2.drawContours(overlay, grass_zones, -1, (0, 0, 120), thickness=-1)
    cv2.addWeighted(overlay, 0.5, frame_display, 0.5, 0, frame_display)
    cv2.drawContours(frame_display, grass_zones, -1, (0, 0, 255), thickness=2)

cv2.imshow("steamic26-cam", frame_display)
cv2.waitKey(0)
cv2.destroyAllWindows()