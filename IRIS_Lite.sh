echo Starting the Iris_Lite camera system.

python ./clips/startCamera.py &
echo Camera util started

python ./clips/main.py &
echo Main util started

python ./clips/detector_pipeline.py &
echo Detector Pipeline started

python ./clips/listener.py &
echo Listener tool started

python ./clips/zone_monitor.py &
echo Zone monitor started

g++ ./Compression/src/main.cpp &
echo All systems started  