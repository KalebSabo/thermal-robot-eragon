"""Three-state command loop for the Eragon test Reflex template."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class CommandState(str, Enum):
    CALIBRATION = "calibration"
    FORWARD = "forward"
    STOP = "stop"


# Loop order: Calibration/Stand → Forward → Stop → repeat
_STATE_ORDER: tuple[CommandState, ...] = (
    CommandState.CALIBRATION,
    CommandState.FORWARD,
    CommandState.STOP,
)


@dataclass
class StateMachine:
    """Minimal FSM that advances through the three Eragon command modes."""

    current: CommandState = CommandState.CALIBRATION
    tick: int = 0

    def advance(self) -> CommandState:
        index = _STATE_ORDER.index(self.current)
        self.current = _STATE_ORDER[(index + 1) % len(_STATE_ORDER)]
        self.tick += 1
        return self.current

    def payload(self) -> str:
        return self.current.value
