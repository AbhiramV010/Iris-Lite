import cv2
import numpy as np

def perFrameGrass(gRan): # TODO finish this

    # check the presence of an moving object on the grass
        # bottom-most part of moving object MUST be on the grass, if not just disregard
    # needs to be moving because people run on grass, bike bikes on grass, but a stationary tree doesn't need to be counted
    
    fgbg = cv2.createBackgroundSubtractorMOG2()
    motion_mask = fgbg.apply(frame)
    _, motion_mask = cv2.threshold(motion_mask, 240, 255, cv2.THRESH_BINARY)  
    combined_mask = cv2.bitwise_and(gRan, motion_mask)

def defineZone(mask):
    contours, _ = cv2.findContours(mask.astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE) 
    print(contours) 

    field = max(contours, key=cv2.contourArea()) 

    epsilon = 0.02 * cv2.arcLength(field, True) 
    approx = cv2.approxPolyDP(field, epsilon, True)

    if len(approx) >= 4 and len(approx) <= 6: # 4 (rects, inc squares), 6 (irr hex, L shape) 
        pass
    
cam = cv2.VideoCapture(0)
ret, frame = cam.read()
hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
grassRange = cv2.inRange(hsv, np.array([40, 40, 40]), np.array([70, 255, 255]))

defineZone(grassRange)

while True:
    if not ret: 
        cam.release()
        cv2.destroyAllWindows()
        cv2.imshow("Camera Feed",frame)


    if cv2.waitKey(1) == ord("x"):
        cam.release()
        cv2.destroyAllWindows()
    

cap.release()
cv2.destroyAllWindows()   