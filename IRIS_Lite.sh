#!/bin/bash

BINARY="./compression_service"
CPP_DIR="./compression"
PYTHON_DIR="./clips"

loading_bar() {
    local label=$1
    local chars="/-\|"
    for i in {1..20}; do
        echo -ne "\r$label [${chars:$((i%4)):1}] $((i*5))%"
        sleep 0.05
    done
    echo -e "\r$label [✔] 100%   "
}

# 1. SMART COMPILATION
if [ ! -f "$BINARY" ] || [ -n "$(find $CPP_DIR -name "*.cpp" -newer "$BINARY")" ]; then
    echo "Compiling..."
    g++ -O3 "$CPP_DIR/main.cpp" "$CPP_DIR/compressor.cpp" "$CPP_DIR/file_watcher.cpp" \
        "$CPP_DIR/roi_processor.cpp" "$CPP_DIR/utils.cpp" \
        -I./Include -o "$BINARY" -lpthread || exit 1
fi

echo "*** IRIS-Lite SYSTEM BOOTING ***"

# Service Launches
nice -n -15 taskset -c 0 python3 "$PYTHON_DIR/main.py" &
loading_bar "Main Service"

taskset -c 1 python3 "$PYTHON_DIR/listener.py" &
loading_bar "Listener"

taskset -c 2 python3 "$PYTHON_DIR/detector_pipeline.py" &
loading_bar "Detector Pipeline"

taskset -c 2 python3 "$PYTHON_DIR/zone_monitor.py" &
loading_bar "Zone Monitor"

taskset -c 3 "$BINARY" &
loading_bar "C++ Compression"

echo "** IRIS Lite BOOT COMPLETE **"
wait