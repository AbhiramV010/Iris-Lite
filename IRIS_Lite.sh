#!/bin/bash

draw_bar() {
    local percent=$(( $1 * 100 / 6 ))
    local filled=$(( $1 * 2 ))
    local empty=$(( 12 - filled ))
    printf "\rProgress: ["
    printf "%${filled}s" | tr ' ' '█'
    printf "%${empty}s" | tr ' ' '░'
    printf "] %d%%" "$percent"
}

echo "Starting Iris Lite"
draw_bar 0

uxterm -T "TOP" -e "top" & 
sleep 1 && draw_bar 1

uxterm -T "Main Tool" -e "python3 ./Decision/main.py" & 
sleep 1 && draw_bar 1

uxterm -T "Camera Utility" -e "python3 ./Decision/startCamera.py" & 
sleep 5 && draw_bar 2

uxterm -T "Detector Pipeline" -e "python3 ./Decision/detector_pipeline.py" & 
sleep 1 && draw_bar 3

uxterm -T "Listener Tool" -e "python3 ./Decision/listener.py" & 
sleep 1 && draw_bar 4

uxterm -T "Zone Monitor" -e "python3 ./Decision/zone_monitor.py" & 
sleep 1 && draw_bar 5

uxterm -T "Compression System" -e "cd Compression/build && sudo ./PerceptualCompressor" &
sleep 1 && draw_bar 6

echo -e "\nAll systems started."