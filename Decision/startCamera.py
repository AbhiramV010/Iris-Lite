import cv2
import numpy as np
from multiprocessing import shared_memory

W, H = 1920, 1080
SHM_NAME = "iris_live_frame"
SIZE = W * H * 3

P_W, P_H = 600, 450
P_X, P_Y = 0, H - P_H 

def init_camera():
    cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
    
    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*'MJPG'))
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, W)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, H)
    
    fourcc = int(cap.get(cv2.CAP_PROP_FOURCC))
    codec = "".join([chr((fourcc >> 8 * i) & 0xFF) for i in range(4)])
    if codec != "MJPG":
        print(f"Warning: Hardware using {codec} instead of MJPG. Stride issues may occur.")
    
    for x in range(0,120):
        pass
    
    return cap


if __name__ == "__main__":
    prev_zone = None
    
    try:
        old_shm = shared_memory.SharedMemory(name=SHM_NAME)
        old_shm.close()
        old_shm.unlink()
    except FileNotFoundError:
        pass 

    cap = init_camera()

    shm = shared_memory.SharedMemory(name=SHM_NAME, create=True, size=SIZE)
    shared_frame = np.ndarray((H, W, 3), dtype=np.uint8, buffer=shm.buf)

    try:
        print("cam util started")
        while True:
            ret, frame = cap.read()
            if not ret or frame is None:
                continue

            if frame.shape[0] != H or frame.shape[1] != W:
                frame = cv2.resize(frame, (W, H))

            frame = np.ascontiguousarray(frame)

            current_zone = frame[P_Y:P_Y+P_H, P_X:P_X+P_W].copy()

            if prev_zone is not None:
                diff = cv2.absdiff(current_zone, prev_zone)
                gray = cv2.cvtColor(diff, cv2.COLOR_BGR2GRAY)
                _, mask = cv2.threshold(gray, 25, 255, cv2.THRESH_BINARY)
                
                frame[P_Y:P_Y+P_H, P_X:P_X+P_W] = 0
                frame[P_Y:P_Y+P_H, P_X:P_X+P_W][mask > 0] = [255, 255, 255]
            else:
                frame[P_Y:P_Y+P_H, P_X:P_X+P_W] = 0

            prev_zone = current_zone
            
            np.copyto(shared_frame, frame)

    except KeyboardInterrupt:
        pass
    finally:
        shm.close()
        shm.unlink()
        cap.release()
