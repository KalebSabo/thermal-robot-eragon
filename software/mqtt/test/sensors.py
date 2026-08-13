"""Mock sensor payloads keyed to the three command states."""

from __future__ import annotations

import math
import time
from typing import Any

from state_machine import CommandState

# Eight servos: 4 DoF per leg (pitch hip, pitch knee, yaw hip, rotation ankle)
_SERVO_NEUTRAL = [90, 90, 90, 90, 90, 90, 90, 90]
_SERVO_FORWARD = [95, 110, 88, 85, 85, 110, 92, 88]
_SERVO_STOP = [90, 95, 90, 92, 90, 95, 90, 92]


def _jitter(base: float, amplitude: float, phase: float) -> float:
    return base + math.sin(phase) * amplitude


def build_sensor_payload(state: CommandState, loop_tick: int) -> dict[str, Any]:
    """Return a JSON-serializable sensor snapshot for the current command state."""
    phase = time.monotonic()
    tick_phase = loop_tick * 0.35

    if state == CommandState.CALIBRATION:
        servos = [round(_jitter(v, 0.4, phase + i)) for i, v in enumerate(_SERVO_NEUTRAL)]
        distance_cm = round(_jitter(120.0, 0.2, phase), 2)
        camera = {"obstacle_ahead": False, "confidence": 0.98}
        velocity_cm_s = 0.0
    elif state == CommandState.FORWARD:
        servos = [round(_jitter(v, 2.5, phase * 2.0 + i)) for i, v in enumerate(_SERVO_FORWARD)]
        distance_cm = round(max(5.0, 120.0 - (loop_tick % 8) * 4.5 + _jitter(0, 0.5, phase)), 2)
        camera = {"obstacle_ahead": distance_cm < 15.0, "confidence": 0.91}
        velocity_cm_s = round(_jitter(8.0, 0.6, tick_phase), 2)
    else:
        servos = [round(_jitter(v, 0.3, phase + i)) for i, v in enumerate(_SERVO_STOP)]
        distance_cm = round(_jitter(45.0, 0.15, phase), 2)
        camera = {"obstacle_ahead": distance_cm < 20.0, "confidence": 0.95}
        velocity_cm_s = 0.0

    return {
        "state": state.value,
        "loop_tick": loop_tick,
        "servos_deg": servos,
        "distance_cm": distance_cm,
        "velocity_cm_s": velocity_cm_s,
        "camera": camera,
    }
