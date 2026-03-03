#just listens for that one capture class and defines the privacy zone
import cv2
from multiprocessing.connection import Listener

def drawRectangle(event, x, y, flags, param):
    global start_point, end_point, drawing
    if event == cv2.EVENT_LBUTTONDOWN: 
        start_point = (x, y)
        drawing = True
    elif event == cv2.EVENT_MOUSEMOVE:
        if drawing: 
            end_point = (x, y)
    elif event == cv2.EVENT_LBUTTONUP:
        drawing = False
        end_point = (x, y)
        
def definePrivacy(cam): # define privacy zone
    global start_point, end_point, drawing, roi_defined
    start_point = end_point = None
    drawing = False

    cv2.namedWindow('define_privacy_zone')
    cv2.setMouseCallback('define_privacy_zone', drawRectangle)

    while True:
        ret, frame = cam.read() 
        if not ret: break
        img = frame.copy() 

        if start_point and end_point:
            cv2.rectangle(img, start_point, end_point, (0, 255, 0), 2)
        cv2.imshow('define_privacy_zone', img)
        if cv2.waitKey(1) & 0xFF == ord('m'):
            break

    cv2.destroyAllWindows()
    
    try: 
        if roi_defined: return [start_point, end_point]
    except: 
        return None

if __name__ == "__main__":
    address = ('127.0.0.1', 8989)
    with Listener(address, authkey=b'1000011') as l:
        print("Scanner process started")
        while True:
            with l.accept() as conn:
                capture = conn.recv() 
                print(f"Time to clip: {capture.startTime} to {capture.endTime}. {capture.trigger} has triggered it!")
            
            cv2.waitKey(1)