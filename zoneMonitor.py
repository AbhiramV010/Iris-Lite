import cv2
import numpy as np

def startCam():
    global cam, ret, frame, hsv, grassRange, fgbg

    frame = cv2.imread("C:\\Users\\abhir\\Downloads\\STEAMIC_TESTOG2.png")

    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    grassRange = cv2.inRange(hsv, np.array([38, 80, 30]), np.array([80, 255, 180]))

def defineZone(mask):
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9))

    cv2.imshow("NO-PROCESSED?",mask)
    
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours: return None
    field = max(contours, key=cv2.contourArea)
    hull = cv2.convexHull(field)

    epsilon = 0.02 * cv2.arcLength(hull, True)
    approx = cv2.approxPolyDP(hull, epsilon, True)
    return approx

def perFrameGrass(): 
    # TODO: 
    # check the presence of an moving object on the grass
        # bottom-most part of moving object MUST be on the grass, if not just disregard
    # needs to be moving because people run on grass, bike bikes on grass, but a stationary tree doesn't need to be counted, ts supposed to be there
    pass 

startCam()
static_grass_zone = defineZone(grassRange)

frame_display = frame.copy()

if static_grass_zone is not None: 
    overlay = frame_display.copy()
    cv2.drawContours(overlay, [static_grass_zone], -1, (0, 0, 120), thickness=-1)
    cv2.addWeighted(overlay, 0.5, frame_display, 0.5, 0, frame_display)
    cv2.drawContours(frame_display, [static_grass_zone], -1, (0,0,255), thickness=2)

cv2.imshow("steamic26-cam", frame_display)
cv2.waitKey(0)
cv2.destroyAllWindows()