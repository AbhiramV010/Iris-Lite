# linux shell (NOT ZSH) script to start the program in REQUIRED order
# does NOT work on macos due to lack of user-level tools for CPU management
# for windows use startCode.bat
#!/bin/bash

pkill -f main.py
pkill -f zone_monitor.py
pkill -f audio_monitor.py

echo "Starting Scanner process"
nice -n -5 python3 main.py &
sleep 2

echo "Starting sound detector"
taskset -c 0 python3 audio_monitor.py &
sleep 2

echo "Starting zone/video monitor"
taskset -c 1,2 python3 zone_monitor.py &
sleep 2

echo "Starting C++ compression tool"
nice -n 19 taskset -c 3 ./your_cpp_script &