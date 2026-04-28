#!/bin/bash

# Process identifiers based on your IRIS-Lite startup script
processes=("zone_monitor.py" "main.py" "PerceptualCompressor" "detector_pipeline.py" "startCamera.py")

cpu_log="/mnt/clipDrive/cpu_usage.csv"
ram_log="/mnt/clipDrive/ram_usage.csv"

# Initialize files with headers
echo "Timestamp,Process,CPU_Usage" > "$cpu_log"
echo "Timestamp,Process,RAM_Usage" > "$ram_log"

interval=40
total_duration=600 # 10 minutes
start_time=$(date +%s)
end_time=$((start_time + total_duration))

echo "Monitoring IRIS-Lite: 5 processes for 10 minutes..."

while [ $(date +%s) -lt $end_time ]; do
    ts=$(date -u +"%Y-%m-%d %H:%M:%S")
    
    for proc in "${processes[@]}"; do
        # Use pgrep -f to find the script in the full command line
        # Use awk to sum values for multi-threaded processes like the Compressor
        stats=$(pgrep -f "$proc" | xargs -r ps -o %cpu=,%mem= | awk '{cpu+=$1; mem+=$2} END {if(NR>0) print cpu, mem}')
        
        if [ -n "$stats" ]; then
            read -r cpu_val ram_val <<< "$stats"
            echo "$ts,$proc,$cpu_val" >> "$cpu_log"
            echo "$ts,$proc,$ram_val" >> "$ram_log"
        fi
    done
    
    sleep $interval
done