import numpy as np
import pyaudio
import collections
import tensorflow.lite as tflite
import librosa
from captureinfo import CaptureClass 

SOC = [0,1,2,3,4,5,6,7] # sounds of concern, have a look below
# 0 -> genuine silence
# 1 -> ambience (source: from an MPV suburban front-porch) 
# 2 -> car screeching/skidding away
# 3 -> screaming
# 4 -> gunshots
# 5 -> glass breaking 
# 6 -> door banging/punching/aggressive-knocking/kicking 

MODEL = "model.tflite"
RATE = 16000 
CHUNK = 4096 # change to 1024 if poor perf, it'll eat resources tho 
THRESH = 0.03  

interpreter = tflite.Interpreter(model_path=MODEL)
interpreter.allocate_tensors()
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

p = pyaudio.PyAudio()
stream = p.open(format=pyaudio.paFloat32, channels=1, rate=RATE,
                input=True, frames_per_buffer=CHUNK)

audio_buffer = collections.deque(maxlen=RATE * 3)

def pre_process(audio_np):
    spec = librosa.feature.melspectrogram(y=audio_np, sr=RATE, n_mels=128)
    log_spec = librosa.power_to_db(spec, ref=np.max)
    return log_spec[:, :98] 

try:
    while True:
        data = stream.read(CHUNK, exception_on_overflow=False)
        chunk = np.frombuffer(data, dtype=np.float32)
        audio_buffer.extend(chunk)

        rms = np.sqrt(np.mean(chunk**2))
        
        # is the noise actually a big deal & do we have more than two seconds of audio
        # if it doesn't meet BOTH (not one) conditions, it'll skip over ALL processing, giving massive saves
        if rms > THRESH and len(audio_buffer) >= (RATE * 2):
            processed_data = pre_process(np.array(audio_buffer))
            input_data = processed_data[np.newaxis, ..., np.newaxis].astype(np.float32)
            interpreter.set_tensor(input_details[0]['index'], input_data)
            interpreter.invoke()
            output_data = interpreter.get_tensor(output_details[0]['index'])
            prediction = np.argmax(output_data)
            confidence = output_data[0][prediction]
            if prediction in SOC and confidence > 0.8: # is the prediction in the "knowledge" and is it confident enough
                print(f"The sound was {prediction}, reported with {confidence:.2f}% confidence")

except KeyboardInterrupt: 
    stream.stop_stream()
    stream.close()
    p.terminate()