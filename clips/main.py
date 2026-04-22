import cv2
import numpy as np
from multiprocessing import shared_memory
from multiprocessing.connection import Listener
import threading
import time
import os
import ctypes
import sys

FPS = 24  
BUFFER_MINUTES = 5 
FRAME_BUFFER_SIZE = FPS * 60 * BUFFER_MINUTES 
SLOT_SIZE = 80000
SHM_NAME = "iris_live_frame"
INDICE_SHM = "iris_frame_indices"
ADDRESS = ('127.0.0.1', 8989)
global LISTENER, CONN
LISTENER = Listener(ADDRESS, authkey=b'1000011')
CONN = LISTENER.accept();

def lock_memory():
    try:
        ctypes.CDLL("libc.so.6").mlockall(1 | 2)
    except Exception: pass

buffer_lock = threading.Lock()

def checkAlert(): # checks if alert happened, then writes to frame_indices shm
    with conn.
    

if __name__ == "__main__":
    lock_memory()
    try:
        shm = shared_memory.SharedMemory(name=SHM_NAME)
        frame_indices = shared_memory.SharedMemory(name=INDICE_SHM, size=16) # 16 bytes, for two uint64 vars in c++
        stream_view = np.ndarray((1080, 1920, 3), dtype=np.uint8, buffer=shm.buf)
    except FileNotFoundError: sys.exit(1)


    shm_names = ["iris_frame_buffer_data", "iris_frame_sizes", "iris_frame_head_tail"]
    sizes = [FRAME_BUFFER_SIZE * SLOT_SIZE, FRAME_BUFFER_SIZE * 4, 16]
    shms = []

    for name, size in zip(shm_names, sizes):
        try: shms.append(shared_memory.SharedMemory(name=name, create=True, size=size))
        except FileExistsError: shms.append(shared_memory.SharedMemory(name=name))

    frame_buffer = np.ndarray((FRAME_BUFFER_SIZE, SLOT_SIZE), dtype=np.uint8, buffer=shms[0].buf)
    frame_sizes = np.ndarray((FRAME_BUFFER_SIZE,), dtype=np.uint32, buffer=shms[1].buf)
    head_tail = np.ndarray((2,), dtype=np.uint64, buffer=shms[2].buf)
    head_tail[0] = head_tail[1] = 0


    ctr=0 # counter for frame indices, VERY important
    while True:
        t_start = time.time()
        _, compressed = cv2.imencode('.jpg', stream_view, [cv2.IMWRITE_JPEG_QUALITY, 25])
        comp_bytes = compressed.tobytes()
        comp_len = len(comp_bytes)
        
        with buffer_lock:
            head = int(head_tail[0])
            frame_buffer[head][:comp_len] = np.frombuffer(comp_bytes, dtype=np.uint8)
            frame_sizes[head] = comp_len
            head_tail[0] = (head + 1) % FRAME_BUFFER_SIZE
            if head_tail[0] == head_tail[1]:
                head_tail[1] = (int(head_tail[1]) + 1) % FRAME_BUFFER_SIZE

        time.sleep(max(1/FPS - (time.time() - t_start), 0.001))