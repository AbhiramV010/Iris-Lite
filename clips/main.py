import cv2
import numpy as np
from multiprocessing import shared_memory
from multiprocessing.connection import Listener
import threading
import time
import os
import ctypes
import sys
from captureinfo import CaptureClass

FPS = 24  
BUFFER_MINUTES = 5 
FRAME_BUFFER_SIZE = FPS * 60 * BUFFER_MINUTES 
SLOT_SIZE = 80000
SHM_NAME = "iris_live_frame"
SHM_NAME_INDICE = "iris_indices"
CONCERN_SHM = "iris_concern_indices"
ADDRESS = ('127.0.0.1', 8989)
AUTHKEY = b'1000011'

capture_queue = []

def findEvents():
    with Listener(ADDRESS, authkey=AUTHKEY) as listener:
        while True:
            with listener.accept() as conn:
                obj = conn.recv()
                if isinstance(obj, CaptureClass):
                        print(str(obj)) # print the __str__ representation, defined in captureinfo.py
                        capture_queue.append(obj)

buffer_lock = threading.Lock()

if __name__ == "__main__":
    ctypes.CDLL("libc.so.6").mlockall(1 | 2) 
      
    event_thread = threading.Thread(target=findEvents, daemon=True)
    event_thread.start()

    shm = shared_memory.SharedMemory(name=SHM_NAME)
    stream_view = np.ndarray((1080, 1920, 3), dtype=np.uint8, buffer=shm.buf)

    shm_names = ["iris_frame_buffer_data", "iris_frame_sizes", "iris_frame_head_tail", CONCERN_SHM, SHM_NAME_INDICE]
    sizes = [FRAME_BUFFER_SIZE * SLOT_SIZE, FRAME_BUFFER_SIZE * 4, 16, 16, FRAME_BUFFER_SIZE * 16]
    shms = []

    for name, size in zip(shm_names, sizes):
        try: shms.append(shared_memory.SharedMemory(name=name, create=True, size=size))
        except FileExistsError: shms.append(shared_memory.SharedMemory(name=name))

    frame_buffer = np.ndarray((FRAME_BUFFER_SIZE, SLOT_SIZE), dtype=np.uint8, buffer=shms[0].buf)
    frame_sizes = np.ndarray((FRAME_BUFFER_SIZE,), dtype=np.uint32, buffer=shms[1].buf)
    head_tail = np.ndarray((2,), dtype=np.uint64, buffer=shms[2].buf)
    concern_indices = np.ndarray((2,), dtype=np.uint64, buffer=shms[3].buf)
    full_indices = np.ndarray((FRAME_BUFFER_SIZE, 2), dtype=np.uint64, buffer=shms[4].buf)
    
    head_tail[0] = head_tail[1] = 0
    global_frame_count = 0

    while True:
        t_start = time.time()

        if capture_queue:
            event = capture_queue.pop(0)
            now = time.time()
            try:
                start_ts = float(event.startTime)
                end_ts = float(event.endTime)
                curr_head = int(head_tail[0])
                
                concern_indices[0] = (curr_head - int((now - start_ts) * FPS)) % FRAME_BUFFER_SIZE
                concern_indices[1] = (curr_head - int((now - end_ts) * FPS)) % FRAME_BUFFER_SIZE
            except: pass

        _, compressed = cv2.imencode('.jpg', stream_view, [cv2.IMWRITE_JPEG_QUALITY, 25])
        comp_bytes = compressed.tobytes()
        comp_len = len(comp_bytes)
        
        with buffer_lock:
            head = int(head_tail[0])
            frame_buffer[head][:comp_len] = np.frombuffer(comp_bytes, dtype=np.uint8)
            frame_sizes[head] = comp_len
            
            full_indices[head] = [int(time.time()), global_frame_count]
            
            head_tail[0] = (head + 1) % FRAME_BUFFER_SIZE
            if head_tail[0] == head_tail[1]:
                head_tail[1] = (int(head_tail[1]) + 1) % FRAME_BUFFER_SIZE
            
            global_frame_count += 1

        time.sleep(max(1/FPS - (time.time() - t_start), 0.001))