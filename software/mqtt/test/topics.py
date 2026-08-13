"""Shared MQTT topic names for the Eragon test state-machine template."""

TOPIC_STATE = "eragon/test/state"
TOPIC_SENSORS = "eragon/test/sensors"
TOPIC_STATUS = "eragon/test/status"

CLIENT_ID_SOLDIER = "eragon-test-soldier"
CLIENT_ID_COMMANDER = "eragon-test-commander"

VALID_STATES = frozenset({"calibration", "forward", "stop"})
