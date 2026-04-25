#!/bin/bash

# Monitor CPU and RAM usage for specified processes

# Processes to monitor
processes=("zone_monitor.py" "main.py" "PerceptualCompressor" "detector_pipeline.py" "startCamera.py")

# CSV files for logging
cpu_log_file="cpu_usage.csv"
ram_log_file="ram_usage.csv"

# Create/clear log files and add header
echo "Timestamp,Process,CPU_Usage" > $cpu_log_file
echo "Timestamp,Process,RAM_Usage" > $ram_log_file

while true; do
    current_time=$(date -u +"%Y-%m-%d %H:%M:%S")
    
    for process in "${processes[@]}"; do
        # Get CPU usage
        cpu_usage=$(ps -C $process -o %cpu=)
        # Get RAM usage
        ram_usage=$(ps -C $process -o %mem=)
        
        if [ -n "$cpu_usage" ]; then
            echo "$current_time,$process,$cpu_usage" >> $cpu_log_file
        fi
        if [ -n "$ram_usage" ]; then
            echo "$current_time,$process,$ram_usage" >> $ram_log_file
        fi
    done
    sleep 60  # Adjust monitoring interval as needed
done