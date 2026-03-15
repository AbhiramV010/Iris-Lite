# Checking for gunshots, glass breaking, screaming, car crashing or car screeching away
import pyaudio
import numpy as np
from scipy import signal
import tensorflow as tf

def spectogramConverter(audio,rate=16000,frameLen=480,frameStep=320):
    f, t, Zxx = signal.stft(audio, fs=rate, nperseg=frameLen, noverlap=frameLen-frameStep)
    return np.abs(Zxx)

mic=pyaudio.PyAudio() 
feed=mic.open(format=pyaudio.paInt16, channels=1, rate=44100, input=True, output=True, frames_per_buffer=1024)

while True:
    spectogramConverter(audio=feed)
