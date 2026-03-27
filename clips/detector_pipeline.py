import cv2
import numpy as np

# init
fgbg= cv2.bgsegm.createBackgroundSubtractorCNT() # little less intensive so the Pi can breathe

cap = cv2.VideoCapture(0)

cap.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)

# Tier 1 -> Basics, like motion anomalies, camera disturbance, and luminence delta
def tier1Actions():
    if not ret: raise OSError("Unable to scan frame from camera. If running on the Pi, check if video inputs were configured right");
    
    fgmask = fgbg.apply(frame);
    cv2.imshow("fgmask",fgmask)
    
def tier2Actions():
    pass

while True:    
    ret, frame = cap.read()
    if cv2.waitKey(1) & 0xFF == ord('x'): break

    if tier1Actions():
        tier2Actions()


# Tier 2 -> More heavy stuff: motion entropy (which is basically predictability), sound-video match up, and persistence


# main loop -> Arduino type