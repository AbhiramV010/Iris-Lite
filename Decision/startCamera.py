import cv2
import numpy as np
from multiprocessing import shared_memory
import sys
import pyaudio
import threading

W, H = 1920, 1080
SHM_NAME = "iris_live_frame"
SIZE = W * H * 3

P_W, P_H = 600, 450
P_X, P_Y = 0, H - P_H 

FPS = 24
AUDIO_RATE = 44100
AUDIO_CHANNELS = 1
AUDIO_FORMAT = pyaudio.paInt16
AUDIO_CHUNK_SIZE = int(AUDIO_RATE / FPS)
AUDIO_SLOT_SIZE = AUDIO_CHUNK_SIZE * 2
AUDIO_FRAME_BUFFER_SIZE = FPS * 60 * 5
AUDIO_SHM_NAME = "iris_audio_buffer"

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

    try:
        old_audio_shm = shared_memory.SharedMemory(name=AUDIO_SHM_NAME)
        old_audio_shm.close()
        old_audio_shm.unlink()
    except FileNotFoundError:
        pass

    cap = init_camera()

    shm = shared_memory.SharedMemory(name=SHM_NAME, create=True, size=SIZE)
    shared_frame = np.ndarray((H, W, 3), dtype=np.uint8, buffer=shm.buf)

    audio_shm = shared_memory.SharedMemory(name=AUDIO_SHM_NAME, create=True, size=AUDIO_FRAME_BUFFER_SIZE * AUDIO_SLOT_SIZE)
    audio_buffer = np.ndarray((AUDIO_FRAME_BUFFER_SIZE, AUDIO_SLOT_SIZE), dtype=np.uint8, buffer=audio_shm.buf)

    audio_head = 0
    audio_lock = threading.Lock()

    pa = pyaudio.PyAudio()
    audio_stream = pa.open(
        format=AUDIO_FORMAT,
        channels=AUDIO_CHANNELS,
        rate=AUDIO_RATE,
        input=True,
        input_device_index=1,
        frames_per_buffer=AUDIO_CHUNK_SIZE
    )

    def audio_loop():
        global audio_head
        while True:
            chunk = audio_stream.read(AUDIO_CHUNK_SIZE, exception_on_overflow=False)
            data = np.frombuffer(chunk, dtype=np.uint8)
            with audio_lock:
                audio_buffer[audio_head % AUDIO_FRAME_BUFFER_SIZE][:len(data)] = data
                audio_head += 1

    audio_thread = threading.Thread(target=audio_loop, daemon=True)
    audio_thread.start()

    try:
        print("cam util started")
        while True:
            ret, frame = cap.read()
            if not ret or frame is None:
                continue

            if frame.shape[0] != H or frame.shape[1] != W:
                frame = cv2.resize(frame, (W, H))

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
        audio_stream.stop_stream()
        audio_stream.close()
        pa.terminate()
        shm.close()
        shm.unlink()
        audio_shm.close()
        audio_shm.unlink()
        cap.release()
