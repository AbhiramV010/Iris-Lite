#!/bin/bash
echo "Timestamp,Script,CPU_Percent,RAM_MB" > process_stats.csv

SCRIPTS=("listener.py" "detector_pipeline.py" "zone_monitor.py" "main.cpp")
# listener.py -> sound detection
# detector_pipeline.py -> two tiered GENERAL detection
# zone_monitor.py -> monitors the presence 
# main.cpp -> c++ compression software

while true; do
    TIMESTAMP=$(date +"%H:%M:%S")
    
    for SCRIPT in "${SCRIPTS[@]}"; do
        PID=$(pgrep -f "$SCRIPT")
        
        if [ -n "$PID" ]; then
            STATS=$(ps -p "$PID" -o %cpu,rss --no-headers)
            CPU=$(echo $STATS | awk '{print $1}')
            RAM_KB=$(echo $STATS | awk '{print $2}')
            RAM_MB=$(echo "scale=2; $RAM_KB / 1024" | bc)
            
            echo "$TIMESTAMP,$SCRIPT,$CPU,$RAM_MB" >> process_stats.csv
        fi
    done

    sleep 30 # 30 sec interval between data recordings
done