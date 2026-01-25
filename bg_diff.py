import cv2
import numpy as np

cap = cv2.VideoCapture(0)
lastgrass = int()

while True:
    ret, frame = cap.read()

    if not ret:
        break

    # action for each frame START
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    lower_green = np.array(
        [40, 40, 40]
    )  ## these define the boundaries of what grass will be classified as
    upper_green = np.array([70, 255, 255])
    cv2.imshow("OpenCV2", frame)

    mask = cv2.inRange(hsv, lower_green, upper_green)
    ## per frame actions
    grasspixl = cv2.countNonZero(mask)

    if lastgrass * 0.25 < grasspixl:
        print("Potential problem detected")

    lastgrass = grasspixl

    cv2.imshow("Grass Mask", mask)

    if cv2.waitKey(1) == ord("x"):
        break


cap.release()
cv2.destroyAllWindows()
