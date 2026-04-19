#!/bin/bash
echo Starting the Iris_Lite camera system.

sleep 1
uxterm -T "Camera Utility" -e "python3 ./clips/startCamera.py" &

sleep 1
uxterm -T "Main Utility" -e "python3 ./clips/main.py" &

sleep 1
uxterm -T "Detector Pipeline" -e "python3 ./clips/detector_pipeline.py" &

sleep 1
uxterm -T "Listener Tool" -e "python3 ./clips/listener.py" &

sleep 1
uxterm -T "Zone Monitor" -e "python3 ./clips/zone_monitor.py" &

sleep 1
uxterm -T "Compression System" -e "sh -c 'g++ ./Compression/src/main.cpp -o ./Compression/src/compressor && ./Compression/src/compressor; exec bash'" &

sleep 1
top

echo All systems started.