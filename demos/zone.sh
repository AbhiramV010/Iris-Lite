#!/bin/bash

draw_bar() {
    local percent=$(( $1 * 100 / 3 ))
    local filled=$(( $1 * 4 ))
    local empty=$(( 12 - filled ))
    printf "\rProgress: [%-12s] %d%%" "$(printf '█%.0s' $(seq 1 $filled))" "$percent"
}

echo "Starting Iris-Lite Zone Monitor"
draw_bar 0

uxterm -T "Main Tool" -e "python3 ./Decision/main.py" & 
sleep 1 && draw_bar 1

uxterm -T "Camera Utility" -e "python3 ./Decision/startCamera.py" & 
sleep 5 && draw_bar 2

uxterm -T "Zone Monitor" -e "python3 ./Decision/zone_monitor.py" & 
sleep 1 && draw_bar 3

echo -e "\nZone Monitor active."