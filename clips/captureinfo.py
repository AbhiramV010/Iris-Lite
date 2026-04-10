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