:: windows batch script to start the program in REQUIRED order
:: works on windows ONLY, for linux/macos use startCode.sh
@echo off
title Security System Bootloader (Admin)

net session >nul 2>&1
if %errorLevel% == 0 (
    goto :START_LOGIC
) else (
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

:START_LOGIC
cd /d "%~dp0"
taskkill /F /IM python.exe /T >nul 2>&1

echo Starting main listener
start "Main_Listener" /high python main.py
timeout /t 2 >nul

echo Starting audio monitor
start "Audio_Monitor" /affinity 1 python audio_monitor.py
timeout /t 2 >nul

echo Starting video monitor
start "Video_Monitor" /affinity 6 python zone_monitor.py
timeout /t 2 >nul

echo Starting c++ compression
start "CPP_Compressor" /low /affinity 8 your_cpp_script.exe

pause