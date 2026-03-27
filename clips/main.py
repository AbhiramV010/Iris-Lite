import cv2
import numpy as np
from multiprocessing.connection import Listener
import threading

start_point = None
end_point = None
drawing = False

def drawRectangle(event, x, y, flags, param):
    global start_point, end_point, drawing
    if event == cv2.EVENT_LBUTTONDOWN:
        start_point, end_point, drawing = (x, y), (x, y), True
    elif event == cv2.EVENT_MOUSEMOVE and drawing:
        end_point = (x, y)
    elif event == cv2.EVENT_LBUTTONUP:
        drawing, end_point = False, (x, y)

def listen_loop(l):
    while True:
        try:
            with l.accept() as conn:
                capture = conn.recv()
        except:
            break

if __name__ == "__main__":
    cam = cv2.VideoCapture(0)
    
    cv2.namedWindow('define_privacy_zone')
    cv2.setMouseCallback('define_privacy_zone', drawRectangle)
    while True:
        ret, frame = cam.read()
        if not ret: break
        if start_point and end_point:
            cv2.rectangle(frame, start_point, end_point, (0, 255, 0), 2)
        cv2.imshow('define_privacy_zone', frame)
        if cv2.waitKey(1) & 0xFF == ord('m'): 
            break
    cv2.destroyWindow('define_privacy_zone')

    ret, frame = cam.read()
    prev_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    address = ('127.0.0.1', 8989)
    try:
        l = Listener(address, authkey=b'1000011')
        print("Scanner process started.")
        threading.Thread(target=listen_loop, args=(l,), daemon=True).start()
        
        while True:
            ret, frame = cam.read()
            if not ret: break

            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            diff = cv2.absdiff(prev_gray, gray)
            _, motion_mask = cv2.threshold(diff, 25, 255, cv2.THRESH_BINARY)
            motion_mask = cv2.dilate(motion_mask, np.ones((3,3), np.uint8), iterations=1)
            prev_gray = gray

            if start_point and end_point:
                x1, y1 = min(start_point[0], end_point[0]), min(start_point[1], end_point[1])
                x2, y2 = max(start_point[0], end_point[0]), max(start_point[1], end_point[1])
                
                if x2 > x1 and y2 > y1:
                    roi = frame[y1:y2, x1:x2]
                    mask_roi = motion_mask[y1:y2, x1:x2]
                    frame[y1:y2, x1:x2] = cv2.bitwise_and(roi, roi, mask=mask_roi)

            # cv2.imshow('steam-ic-2026', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'): break

    except Exception as e:
        raise OSError(f"Something happened while trying to send the class. \n{e}")
    finally:
        cam.release()
        cv2.destroyAllWindows()