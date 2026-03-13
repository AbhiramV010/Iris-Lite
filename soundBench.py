# END GOAL: Checking for gunshots, will implement the rest into sound_monitor.py AFTER 
# CURRENTLY DOES: Mirrors back the same audio
import pyaudio

mic=pyaudio.PyAudio() 
audio=mic.open(format=pyaudio.paInt16, channels=1, rate=44100, input=True, output=True, frames_per_buffer=1024)

try:
    while True:
        data=audio.read(1024)
        audio.write(data)
except KeyboardInterrupt: 
    audio.stop_stream()
    audio.close()
    mic.terminate()