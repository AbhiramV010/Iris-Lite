import cv2
import numpy as np

def startCam():
    global cam, ret, frame, hsv, grassRange, fgbg

    # fgbg = cv2.bgsegm.createBackgroundSubtractorCNT() # algorithm that already exists, using standard deviation to identify changes
    cam = cv2.VideoCapture(0)
    
    for _ in range(30): cam.read() 
    
    ret, frame = cam.read()
    if not ret: return
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    grassRange = cv2.inRange(hsv, np.array([30, 70, 20]), np.array([90, 255, 255]))

def defineZone(mask):
    kernel = np.ones((9,9), np.uint8) # optimal matrice dimensions determined using a pixel ruler and sample image 

    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel) # NOTE: do not make the kernel bigger, use iteration with the same kernel 
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
    
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours: return None
    field = max(contours, key=cv2.contourArea) # simplify object from 1000s of vertices to far less
    epsilon = 0.02 * cv2.arcLength(field, True)
    approx = cv2.approxPolyDP(field, epsilon, True)
    cv2.imshow("hsvCam",hsv)

    return approx

def perFrameGrass(): 
    # TODO:
    # check the presence of an moving object on the grass
        # bottom-most part of moving object MUST be on the grass, if not just disregard
    # needs to be moving because people run on grass, bike bikes on grass, but a stationary tree doesn't need to be counted, ts supposed to be there
    pass 


startCam()
static_grass_zone = defineZone(grassRange)

while True:
    ret, frame = cam.read()
    if static_grass_zone is not None: 
        overlay = frame.copy()
        cv2.drawContours(overlay, [static_grass_zone], -1, (220, 220, 220), thickness=-1)
        cv2.addWeighted(overlay, 0.5, frame, 0.5, 0, frame)
        cv2.drawContours(frame, [static_grass_zone], -1, (255, 255, 255), thickness=2)  
    if not ret: break

    cv2.imshow("steamic26-cam", frame)
    
    if cv2.waitKey(1) == ord("x"): break 
    
cam.release()
cv2.destroyAllWindows()
