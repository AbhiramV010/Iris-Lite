# this file simply has the class, which other files will make objects out of and send to main.py and compress.cpp
import time
from datetime import *

class CaptureClass:
    def __init__(self,startTime,endTime,trigger):
        self.startTime=startTime
        self.endTime=endTime
        self.trigger=trigger

    def run_checks(self):
        if self.startTime > self.endTime: raise Exception(f"startTime ({self.startTime} is after {self.endTime})") 
    
    def __str__(self):
        return()