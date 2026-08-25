import cv2
import numpy as np
import struct
from multiprocessing import shared_memory
from multiprocessing.connection import Listener
import threading
import time
import datetime
from captureinfo import CaptureClass
from secrets_util import load_authkey, decrypt_capture
import warnings

FPS = 24
BUFFER_MINUTES = 5
FRAME_BUFFER_SIZE = FPS * 60 * BUFFER_MINUTES
SLOT_SIZE = 200000
SHM_NAME = "iris_live_frame"
SHM_NAME_INDICE = "iris_indices"
CONCERN_SHM = "iris_concern_indices"
ADDRESS = ('127.0.0.1', 8989)
AUTHKEY = load_authkey()

# Mirrors Compression/src/main.cpp's `SharedEventBuffer { uint64_t startFrame;
# uint64_t endFrame; char trigger[64]; }` exactly (no padding: both uint64
# fields are already 8-byte aligned, and 80 is a multiple of 8) so the two
# languages agree on the layout of the "iris_concern_indices" segment.
CONCERN_STRUCT = struct.Struct("<QQ64s")

# Pickled CaptureClass objects are well under a few hundred bytes; cap well
# above that so a malformed/hostile sender can't force an unbounded
# allocation before decrypt_capture() even runs.
MAX_EVENTBUS_MSG = 4096

capture_queue = []

def findEvents():
    with Listener(ADDRESS, authkey=AUTHKEY) as listener:
        while True:
            with listener.accept() as conn:
                try:
                    blob = conn.recv_bytes(maxlength=MAX_EVENTBUS_MSG)
                except (OSError, EOFError):
                    continue # oversized, truncated, or otherwise unusable message
                try:
                    obj = decrypt_capture(blob, AUTHKEY)
                except Exception:
                    continue # tampered/malformed payload, drop it
                if isinstance(obj, CaptureClass):
                        print(str(obj)) # print the __str__ representation, defined in captureinfo.py
                        capture_queue.append(obj)

buffer_lock = threading.Lock()

if __name__ == "__main__":
    
    while True:
        try:
            shm = shared_memory.SharedMemory(name=SHM_NAME)
            break
        except FileNotFoundError:
                time.sleep(0.5)
      
    event_thread = threading.Thread(target=findEvents, daemon=True)
    event_thread.start()

    try: shm = shared_memory.SharedMemory(name=SHM_NAME)
    except FileNotFoundError: shm = shared_memory.SharedMemory(name=SHM_NAME, create=True, size=1920 * 1080 * 3)

    time.sleep(1)
    stream_view = np.ndarray((1080, 1920, 3), dtype=np.uint8, buffer=shm.buf)

    shm_names = ["iris_frame_buffer_data", "iris_frame_sizes", "iris_frame_head_tail", CONCERN_SHM, SHM_NAME_INDICE]
    sizes = [FRAME_BUFFER_SIZE * SLOT_SIZE, FRAME_BUFFER_SIZE * 4, 16, CONCERN_STRUCT.size, FRAME_BUFFER_SIZE * 16]
    shms = []

    for name, size in zip(shm_names, sizes):
        try: shms.append(shared_memory.SharedMemory(name=name, create=True, size=size))
        except FileExistsError: shms.append(shared_memory.SharedMemory(name=name))

    print("main util started")
    frame_buffer = np.ndarray((FRAME_BUFFER_SIZE, SLOT_SIZE), dtype=np.uint8, buffer=shms[0].buf)
    frame_sizes = np.ndarray((FRAME_BUFFER_SIZE,), dtype=np.uint32, buffer=shms[1].buf)
    head_tail = np.ndarray((2,), dtype=np.uint64, buffer=shms[2].buf)
    concern_buf = shms[3].buf
    full_indices = np.ndarray((FRAME_BUFFER_SIZE, 2), dtype=np.uint64, buffer=shms[4].buf)
    
    head_tail[0] = head_tail[1] = 0
    global_frame_count = 0

    while True:
        t_start = time.time()

        try: cv2.imshow("Iris-Lite Camera Feed", stream_view)
        except: warnings.warn("No display detected, will run headlessly")
        
        if cv2.waitKey(1) & 0xFF == ord('x'):
            break

        if capture_queue:
            event = capture_queue.pop(0)
            now = time.time()
            try:
                today = datetime.date.today()
                start_ts = datetime.datetime.combine(today, datetime.time.fromisoformat(event.startTime)).timestamp()
                end_ts = datetime.datetime.combine(today, datetime.time.fromisoformat(event.endTime)).timestamp()
                curr_head = int(head_tail[0])

                padding_frames = 5 * FPS
                start_offset = int((now - start_ts) * FPS) + padding_frames
                end_offset = int((now - end_ts) * FPS) - padding_frames

                start_idx = (curr_head - max(0, start_offset)) % FRAME_BUFFER_SIZE
                end_idx = (curr_head - max(0, end_offset)) % FRAME_BUFFER_SIZE
                # Truncate to 63 bytes so the struct's zero-padding always
                # leaves the C string NUL-terminated within char[64].
                trigger_bytes = event.trigger.encode("utf-8", "replace")[:63]
                CONCERN_STRUCT.pack_into(concern_buf, 0, start_idx, end_idx, trigger_bytes)
            except: pass

        _, compressed = cv2.imencode('.jpg', stream_view, [cv2.IMWRITE_JPEG_QUALITY, 45])
        comp_bytes = compressed.tobytes()
        comp_len = len(comp_bytes)

        if comp_len > SLOT_SIZE:
            _, compressed = cv2.imencode('.jpg', stream_view, [cv2.IMWRITE_JPEG_QUALITY, 30])
            comp_bytes = compressed.tobytes()
            comp_len = len(comp_bytes)
            if comp_len > SLOT_SIZE:
                time.sleep(max(1/FPS - (time.time() - t_start), 0.001))
                continue
        
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