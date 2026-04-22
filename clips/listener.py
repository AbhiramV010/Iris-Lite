import numpy as np
import pyaudio
import collections
import ai_edge_litert.interpreter as litert
import librosa
from multiprocessing.connection import Client
import os
from datetime import datetime, timedelta
from captureinfo import CaptureClass
from sensor_helper import *
import warnings

warnings.simplefilter('ignore', Warning) 
SOUND_LABELS = {1: "Ambience", 2: "Car Screech", 3: "Screaming", 4: "Gunshot", 5: "Glass Breaking", 6: "Aggressive Knocking", 7: "Dog Barking"}
SOC = [2, 3, 4, 5, 6, 7] 

MODEL = os.path.join(os.path.dirname(__file__), "sound_model.tflite")
RATE = 16000 
CHUNK = 4096 
ADDRESS = ('127.0.0.1', 8989)
AUTHKEY = b'1000011'
THRESHOLD = 0.08 

interpreter = litert.Interpreter(model_path=MODEL)
interpreter.allocate_tensors()
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

p = pyaudio.PyAudio()
device_index = None

for i in range(p.get_device_count()):
    dev_info = p.get_device_info_by_index(i)
    if "default" in dev_info['name'] or "dsnoop" in dev_info['name']:
        device_index = i
        break

dev_info = p.get_device_info_by_index(device_index)
stream = p.open(format=pyaudio.paFloat32, 
                channels=(p.get_device_info_by_index(device_index))['maxInputChannels'], 
                rate=RATE,
                input=True, 
                input_device_index=device_index,
                frames_per_buffer=CHUNK)

audio_buffer = collections.deque(maxlen=RATE * 3)

def pre_process(audio_np):
    audio_np = librosa.util.normalize(audio_np)
    spec = librosa.feature.melspectrogram(y=audio_np, sr=RATE, n_mels=128, hop_length=327)
    log_spec = librosa.power_to_db(spec, ref=1.0)
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

                if prediction in SOC and confidence > 0.6: 
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
                    
                    new_capture = CaptureClass(
                        startTime=buffered_start.strftime("%H:%M:%S"), 
                        endTime=buffered_end.strftime("%H:%M:%S"),
                        trigger=f"{current_label} ({max_confidence*100:.1f}%)",
                        duration=round(total_duration, 2),
                        isMotionSensor=check_gpio(27), 
                        isDoorSensor=check_gpio(17)
                    )
                    print(f"\nCaptured: {new_capture.trigger}")

                    try:
                        with Client(ADDRESS, authkey=AUTHKEY) as conn:
                            conn.send(new_capture)
                    except:
                        pass
                    
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