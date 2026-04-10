#!/bin/bash

CYAN='\033[0;36m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

draw_progress_bar() {
    local duration=$1
    local width=40
    for ((progress=0; progress<=width; progress++)); do
        let "percent = progress * 100 / width"
        printf "\r${CYAN}Initializing IRIS-Lite [${GREEN}"
        for ((i=0; i<progress; i++)); do printf "━"; done
        for ((i=progress; i<width; i++)); do printf " "; done
        printf "${CYAN}] %d%%${NC}" $percent
        sleep $(echo "$duration / $width" | bc -l)
    done
    echo -e "\n"
}

if [ ! -f "./build/PerceptualCompressor" ]; then
    echo -e "${CYAN}Binary not found. Compiling PerceptualCompressor...${NC}"
    mkdir -p build
    cd build || exit
    cmake .. && make -j$(nproc)
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Build successful.${NC}"
        cd ..
    else
        echo -e "${RED}Build failed! Check your CMakeLists.txt and dependencies.${NC}"
        exit 1
    fi
fi

python3 clips/listener.py > listener.log 2>&1 &
python3 clips/detector_pipeline.py > pipeline.log 2>&1 &
python3 clips/zone_monitor.py > zone.log 2>&1 &
python3 clips/main.py > main_py.log 2>&1 &

draw_progress_bar 1.5

echo -e "${GREEN}Clip Software Started${NC}"
./build/PerceptualCompressor

trap "kill 0" EXIT