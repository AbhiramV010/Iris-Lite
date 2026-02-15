import cv2
import numpy as np

def startCam():
    global cam, ret, frame, hsv, grassRange, fgbg

    fgbg = cv2.createBackgroundSubtractorKNN() # algorithm that already exists, using standard deviation to identify changes
    cam = cv2.VideoCapture(0)
    for _ in range(60): cam.read() 
    
    ret, frame = cam.read()
    if not ret: return
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    grassRange = cv2.inRange(hsv, np.array([35, 40, 40]), np.array([85, 255, 255]))    

def defineZone(mask):
    kernel = np.ones((3,3), np.uint8)
    
    mask=cv2.dilate(mask,kernel,iterations=1)
    mask = cv2.erode(mask, kernel, iterations=2)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)
    mask = cv2.dilate(mask, kernel, iterations=2)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=4)
    
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours: return None

    min_area = (frame.shape[0] * frame.shape[1]) * 0.05
    valid_contours = [c for c in contours if cv2.contourArea(c) > min_area] # filter out any green patches that take up less than 5% of screen space 

    if not valid_contours: return None

    field = max(valid_contours, key=cv2.contourArea)
    epsilon = 0.02 * cv2.arcLength(field, True) 
    approx = cv2.approxPolyDP(field, epsilon, True)

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
        cv2.drawContours(overlay, [static_grass_zone], -1, (220, 120, 120), thickness=-1)
        cv2.addWeighted(overlay, 0.5, frame, 0.5, 0, frame)
        cv2.drawContours(frame, [static_grass_zone], -1, (255, 155, 155), thickness=2)  
    if not ret: break

    cv2.imshow("steamic26-cam", frame)
    
    if cv2.waitKey(1) == ord("x"): break 
    
cam.release()
cv2.destroyAllWindows()
