import numpy as np
import pyaudio
import collections
import ai_edge_litert.interpreter as litert
from multiprocessing.connection import Client
from multiprocessing import shared_memory
import os
import time
from datetime import datetime, timedelta
from captureinfo import CaptureClass
from sensor_helper import *
import warnings
import sys

warnings.simplefilter('ignore', Warning) 
SOUND_LABELS = {1: "Ambience", 2: "Car Screech", 3: "Screaming", 4: "Gunshot", 5: "Glass Breaking", 6: "Aggressive Knocking", 7: "Dog Barking"}
SOC = [2, 3, 4, 5, 6, 7]  # ambience isn't concerning WHATSOEVER, dog barking is a grey zone, but included to be safe

MODEL = os.path.join(os.path.dirname(__file__), "sound_model.tflite")
RATE = 16000 
CHUNK = 4096 
ADDRESS = ('127.0.0.1', 8989)
AUTHKEY = b'1000011'
THRESHOLD = 0.08 # DB SPL threshold, approx 72 dB

AUDIO_RATE = 44100
AUDIO_CHUNK_SIZE = int(AUDIO_RATE / 24)
AUDIO_SLOT_SIZE = AUDIO_CHUNK_SIZE * 2
AUDIO_FRAME_BUFFER_SIZE = 24 * 60 * 5
AUDIO_SHM_NAME = "iris_audio_buffer"

interpreter = litert.Interpreter(model_path=MODEL)
interpreter.allocate_tensors()
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

p = pyaudio.PyAudio()
device_index = None

for i in range(p.get_device_count()):
    dev_info = p.get_device_info_by_index(i)
    if dev_info['maxInputChannels'] > 0:
        if "usb" in dev_info['name'].lower() or "hw" in dev_info['name'].lower():
            device_index = i
            break

if device_index is None:
    try:
        device_index = p.get_default_input_device_info()['index']
    except:
        sys.exit(1)

try:
    stream = p.open(format=pyaudio.paFloat32, channels=1, rate=RATE, input=True, input_device_index=device_index, frames_per_buffer=CHUNK)
except:
    stream = p.open(format=pyaudio.paFloat32, channels=2, rate=RATE, input=True, input_device_index=device_index, frames_per_buffer=CHUNK)

try:
    audio_shm = shared_memory.SharedMemory(name=AUDIO_SHM_NAME, create=True, size=AUDIO_FRAME_BUFFER_SIZE * AUDIO_SLOT_SIZE)
except FileExistsError:
    audio_shm = shared_memory.SharedMemory(name=AUDIO_SHM_NAME)

audio_shm_buf = np.ndarray((AUDIO_FRAME_BUFFER_SIZE, AUDIO_SLOT_SIZE), dtype=np.uint8, buffer=audio_shm.buf)
audio_slot = 0

audio_buffer = collections.deque(maxlen=RATE * 3)

def get_mel_filters(sr, n_fft, n_mels):
    def hz_to_mel(hz): return 2595 * np.log10(1 + hz / 700.0)
    def mel_to_hz(mel): return 700 * (10**(mel / 2595.0) - 1)
    mel_pts = np.linspace(hz_to_mel(0), hz_to_mel(sr / 2), n_mels + 2)
    hz_pts = mel_to_hz(mel_pts)
    bin_pts = np.floor((n_fft + 1) * hz_pts / sr).astype(int)
    filters = np.zeros((n_mels, n_fft // 2 + 1))
    for i in range(1, n_mels + 1):
        filters[i-1, bin_pts[i-1]:bin_pts[i]] = (np.arange(bin_pts[i-1], bin_pts[i]) - bin_pts[i-1]) / (bin_pts[i] - bin_pts[i-1])
        filters[i-1, bin_pts[i]:bin_pts[i+1]] = (bin_pts[i+1] - np.arange(bin_pts[i], bin_pts[i+1])) / (bin_pts[i+1] - bin_pts[i])
    return filters

def pre_process(audio_np):
    audio_np = audio_np / (np.max(np.abs(audio_np)) + 1e-9)
    n_fft, n_mels, hop_length = 2048, 128, 327
    window = np.hanning(n_fft)
    frames = np.array([audio_np[i:i+n_fft] for i in range(0, len(audio_np)-n_fft, hop_length)])
    
    stft = np.abs(np.fft.rfft(frames * window, n=n_fft))**2
    filters = get_mel_filters(RATE, n_fft, n_mels)
    spec = np.dot(stft, filters.T).T
    log_spec = 10 * np.log10(np.maximum(1e-10, spec))
    log_spec = (log_spec - np.min(log_spec)) / (np.max(log_spec) - np.min(log_spec) + 1e-6)
    
    if log_spec.shape[1] > 98:
        log_spec = log_spec[:, :98]
    elif log_spec.shape[1] < 98:
        log_spec = np.pad(log_spec, ((0, 0), (0, 98 - log_spec.shape[1])), mode='constant')
    return log_spec

active_detection = False
detection_start_time = None
current_label = None
max_confidence = 0.0

try:
    try: 
        start_up(17)
        start_up(27)
    except: pass
    while True:
        data = stream.read(CHUNK, exception_on_overflow=False)
        chunk = np.frombuffer(data, dtype=np.float32)
        
        if stream._channels == 2:
            chunk = chunk.reshape(-1, 2).mean(axis=1)

        # write to shm as int16 PCM
        pcm = (chunk * 32768.0).astype(np.int16).view(np.uint8)
        slot = audio_slot % AUDIO_FRAME_BUFFER_SIZE
        audio_shm_buf[slot][:len(pcm)] = pcm
        audio_slot += 1

        current_volume = np.sqrt(np.mean(chunk**2))
        print(f"Volume: {current_volume:.5f} | Trigger: {current_volume > THRESHOLD}", end='\r')
        
        if current_volume > THRESHOLD:
            audio_buffer.extend(chunk)
            if len(audio_buffer) >= (RATE * 2):
                audio_array = np.array(list(audio_buffer))
                recent_audio = audio_array[-(RATE * 2):]
                processed_data = pre_process(recent_audio)
                
                input_data = processed_data[np.newaxis, ..., np.newaxis].astype(np.float32)
                interpreter.set_tensor(input_details[0]['index'], input_data)
                interpreter.invoke()
                
                output_data = interpreter.get_tensor(output_details[0]['index'])
                prediction = np.argmax(output_data)
                confidence = float(output_data[0][prediction])

                if prediction in SOC and confidence >= 0.45: # confidence is minimum 45%
                    if not active_detection:
                        active_detection = True
                        detection_start_time = datetime.now()
                        current_label = SOUND_LABELS.get(prediction)
                        max_confidence = confidence
                    
                    else:
                        max_confidence = max(max_confidence, confidence)
                
                elif active_detection:
                    detection_end_time = datetime.now()
                    buffered_start = detection_start_time - timedelta(seconds=5)
                    buffered_end = detection_end_time + timedelta(seconds=5)
                    total_duration = (buffered_end - buffered_start).total_seconds()
                    
                    new_capture = CaptureClass(startTime=buffered_start.strftime("%H:%M:%S"), endTime=buffered_end.strftime("%H:%M:%S"), trigger=f"{current_label} ({max_confidence*100:.1f}%)", duration=round(total_duration, 2), isMotionSensor=check_gpio(27), isDoorSensor=check_gpio(17))
                    print(f"\nCaptured: {new_capture.trigger}")
                    
                    with Client(ADDRESS, authkey=AUTHKEY) as conn:
                        conn.send(new_capture)
                    active_detection = False
                    max_confidence = 0.0
except KeyboardInterrupt: 
    stream.stop_stream()
    stream.close()
    p.terminate()
finally:
    try:
        close_gpio(17)
        close_gpio(27)
    except: pass
    audio_shm.close()
    audio_shm.unlink()