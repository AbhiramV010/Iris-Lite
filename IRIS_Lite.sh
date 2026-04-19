#!/bin/bash
echo Starting the Iris_Lite camera system.

uxterm -T "Camera Utility" -e "python3 ./clips/startCamera.py" &
uxterm -T "Main Utility" -e "python3 ./clips/main.py" &
uxterm -T "Detector Pipeline" -e "python3 ./clips/detector_pipeline.py" &
uxterm -T "Listener Tool" -e "python3 ./clips/listener.py" &
uxterm -T "Zone Monitor" -e "python3 ./clips/zone_monitor.py" &

uxterm -T "Compression System" -e "sh -c 'g++ ./Compression/src/main.cpp -o ./Compression/src/compressor && ./Compression/src/compressor; exec bash'" &

top

echo All systems started.