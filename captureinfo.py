from dataclasses import dataclass
from typing import Optional


@dataclass
class CaptureClass:
    startTime: str
    endTime: str
    label: str
    duration: Optional[float] = None
    zone_id: Optional[int] = None

    importance: Optional[float] = None
    motion_energy: Optional[float] = None
    overlap_stability: Optional[float] = None
    frame_count: Optional[int] = None