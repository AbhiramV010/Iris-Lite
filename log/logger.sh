#!/bin/bash
# IRIS-Lite Performance Logger (Saves to USB)

# Path to your mounted USB
USB_PATH="clipDrive"

# Ensure the directory exists before writing
mkdir -p "$USB_PATH"

# Create headers for 3 separate CSVs on the USB
echo "Timestamp,Script,CPU_Percent,RAM_MB" > "$USB_PATH/process_stats.csv"
echo "Timestamp,Temp_C" > "$USB_PATH/system_temps.csv"
echo "Timestamp,Total_RAM_Used_MB,Swap_Used_MB" > "$USB_PATH/system_ram_usage.csv"

SCRIPTS=("listener.py" "detector_pipeline.py" "zone_monitor.py" "main.py" "startCamera.py")

# End time (10 minutes = 600 seconds)
end=$((SECONDS + 600))

echo "Logging started. Sit tight for 10 minutes..."
echo "Data is being saved to: $(pwd)/$USB_PATH"

while [ $SECONDS -lt $end ]; do
    TIMESTAMP=$(date +"%H:%M:%S")
    
    # 1. Per-Process Stats
    for SCRIPT in "${SCRIPTS[@]}"; do
        PID=$(pgrep -f "$SCRIPT" | head -n 1)
        if [ -n "$PID" ]; then
            STATS=$(ps -p "$PID" -o %cpu,rss --no-headers)
            CPU=$(echo $STATS | awk '{print $1}')
            RAM_KB=$(echo $STATS | awk '{print $2}')
            RAM_MB=$(echo "scale=2; $RAM_KB / 1024" | bc)
            echo "$TIMESTAMP,$SCRIPT,$CPU,$RAM_MB" >> "$USB_PATH/process_stats.csv"
        fi
    done

    # 2. System Temperature
    TEMP=$(vcgencmd measure_temp | tr -d "temp='" | tr -d "'C")
    echo "$TIMESTAMP,$TEMP" >> "$USB_PATH/system_temps.csv"

    # 3. Combined RAM and Swap
    MEM_LINE=$(free -m | awk 'NR==2{used_ram=$3} NR==3{used_swap=$3; print used_ram "," used_swap}')
    echo "$TIMESTAMP,$MEM_LINE" >> "$USB_PATH/system_ram_usage.csv"

    sleep 10
done

echo "Done! You can now unplug the USB and move the CSVs to Excel."