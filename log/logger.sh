#!/bin/bash

processes=("zone_monitor.py" "main.py" "PerceptualCompressor" "detector_pipeline.py" "startCamera.py")

interval=30
total_duration=600
start_time=$(date +%s)
end_time=$((start_time + total_duration))
counter=1

while [ $(date +%s) -lt $end_time ]; do
    echo "$counter"
    ts=$(date +"%H:%M:%S")
    
    for proc in "${processes[@]}"; do
        cpu_val=$(pgrep -f "$proc" | xargs -r ps -o %cpu= | awk '{sum+=$1} END {if(NR>0) print sum}')
        
        if [ -n "$cpu_val" ]; then
            printf "[%s] %-20s CPU: %s%%\n" "$ts" "$proc" "$cpu_val"
        fi
    done
    
    echo "------------------------------------------"
    ((counter++))
    sleep $interval
done