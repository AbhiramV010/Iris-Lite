#!/bin/bash

# Monitor CPU and RAM usage for specified processes

# Processes to monitor (script names to match in full command line)
processes=("zone_monitor.py" "main.py" "PerceptualCompressor" "detector_pipeline.py" "startCamera.py")

# CSV files for logging
cpu_log_file="/mnt/clipDrive/cpu_usage.csv"
ram_log_file="/mnt/clipDrive/ram_usage.csv"

# Create/clear log files and add header (only once at startup)
if [ ! -f "$cpu_log_file" ]; then
    echo "Timestamp,Process,CPU_Usage" > "$cpu_log_file"
fi
if [ ! -f "$ram_log_file" ]; then
    echo "Timestamp,Process,RAM_Usage" > "$ram_log_file"
fi

while true; do
    current_time=$(date -u +"%Y-%m-%d %H:%M:%S")
    
    for process in "${processes[@]}"; do
        # Get CPU usage using pgrep to match full command line (handles python3 scripts)
        cpu_usage=$(pgrep -f "$process" | xargs -r ps -p {} -o %cpu= 2>/dev/null | awk '{sum+=$1} END {print sum}')
        
        # Get RAM usage using pgrep to match full command line (handles python3 scripts)
        ram_usage=$(pgrep -f "$process" | xargs -r ps -p {} -o %mem= 2>/dev/null | awk '{sum+=$1} END {print sum}')
        
        if [ -n "$cpu_usage" ] && [ "$cpu_usage" != "0" ]; then
            echo "$current_time,$process,$cpu_usage" >> "$cpu_log_file"
        fi
        if [ -n "$ram_usage" ] && [ "$ram_usage" != "0" ]; then
            echo "$current_time,$process,$ram_usage" >> "$ram_log_file"
        fi
    done
    sleep 40  # Adjust monitoring interval as needed
done
