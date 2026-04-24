# This is not intended for execution.
from dataclasses import dataclass
from typing import Optional

@dataclass
class CaptureClass:
    startTime: str
    endTime: str
    trigger: str
    duration: Optional[float] = None
    isMotionSensor: Optional[bool] = None
    isDoorSensor: Optional[bool] = None

    def __str__(self):
        return (f""" start: {self.startTime} | end: {self.endTime} | motion: {self.isMotionSensor} |
                     door {self.isDoorSensor} | duration {self.duration} | trigger {self.trigger}""")

if __name__ == "__main__":
    raise RuntimeError("This isn't meant to be executed")