import tensorflow as tf
import pyaudio
import numpy as np
import librosa

p = pyaudio.PyAudio()
stream = p.open(format=pyaudio.paFloat32, channels=1, rate=16000, input=True, frames_per_buffer=1024, input_device_index=1)

def preProcessor(audio):
    print("processed")
    mel_spec = librosa.feature.melspectrogram(y=audio, sr=16000, n_mels=128)
    log_mel_spec = librosa.power_to_db(mel_spec, ref=np.max) # short time fourier transform
    return log_mel_spec

stream.start_stream()

while True:
    data = stream.read(2048, exception_on_overflow=False)
    audio_data = np.frombuffer(data, dtype=np.float32).reshape(-1, 2)
    mono_data = audio_data[:, 0]
    
    processedAudio = preProcessor(mono_data)