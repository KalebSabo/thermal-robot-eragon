#!/usr/bin/env python3
"""
Eragon test Reflex — three-state FSM over MQTT.

Cycles Calibration/Stand → Forward → Stop, publishes mock sensor data to the
Medulla-style commander subscriber.

Usage:
  python3 soldier_fsm.py
  MQTT_BROKER=127.0.0.1 STATE_DWELL_S=3 python3 soldier_fsm.py
"""

from __future__ import annotations

import json
import logging
import os
import signal
import sys
import time
from datetime import datetime, timezone
from typing import Any

import paho.mqtt.client as mqtt

from sensors import build_sensor_payload
from state_machine import CommandState, StateMachine
from topics import (
    CLIENT_ID_SOLDIER,
    TOPIC_SENSORS,
    TOPIC_STATE,
    TOPIC_STATUS,
)

BROKER = os.environ.get("MQTT_BROKER", "127.0.0.1")
PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_USER = os.environ.get("MQTT_USER", "")
MQTT_PASSWORD = os.environ.get("MQTT_PASSWORD", "")
STATE_DWELL_S = float(os.environ.get("STATE_DWELL_S", "3"))
SENSOR_INTERVAL_S = float(os.environ.get("SENSOR_INTERVAL_S", "0.5"))

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
log = logging.getLogger("eragon.test.soldier")


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def build_client() -> mqtt.Client:
    try:
        client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id=CLIENT_ID_SOLDIER,
            protocol=mqtt.MQTTv311,
        )
    except (AttributeError, TypeError):
        client = mqtt.Client(client_id=CLIENT_ID_SOLDIER, protocol=mqtt.MQTTv311)

    if MQTT_USER:
        client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
    return client


def publish_state(client: mqtt.Client, sm: StateMachine) -> None:
    payload = sm.payload()
    ok = client.publish(TOPIC_STATE, payload, qos=1, retain=True)
    log.info("STATE → %s (%s)", payload.upper(), "ok" if ok.rc == 0 else "fail")


def publish_sensors(client: mqtt.Client, sm: StateMachine) -> None:
    sensors = build_sensor_payload(sm.current, sm.tick)
    record = {
        "timestamp": utc_now_iso(),
        "topic": TOPIC_SENSORS,
        **sensors,
    }
    body = json.dumps(record)
    ok = client.publish(TOPIC_SENSORS, body, qos=0, retain=False)
    log.info(
        "SENSORS state=%s dist=%scm vel=%scm/s (%s)",
        sensors["state"],
        sensors["distance_cm"],
        sensors["velocity_cm_s"],
        "ok" if ok.rc == 0 else "fail",
    )


def connect_client(client: mqtt.Client) -> bool:
    log.info("Connecting to broker %s:%s as %s", BROKER, PORT, CLIENT_ID_SOLDIER)
    client.will_set(TOPIC_STATUS, "offline", qos=1, retain=True)
    try:
        client.connect(BROKER, PORT, keepalive=60)
    except OSError as exc:
        log.error("Cannot reach MQTT broker — %s", exc)
        return False

    client.loop_start()
    client.publish(TOPIC_STATUS, "online", qos=1, retain=True)
    return True


def run_state_actions(state: CommandState) -> None:
    if state == CommandState.CALIBRATION:
        log.info("ACTION calibrate/stand — hold neutral pose")
    elif state == CommandState.FORWARD:
        log.info("ACTION forward — advance gait cycle")
    else:
        log.info("ACTION stop — hold and zero velocity")


def main() -> int:
    log.info(
        "Eragon test soldier starting (dwell=%ss, sensor=%ss)",
        STATE_DWELL_S,
        SENSOR_INTERVAL_S,
    )

    sm = StateMachine()
    client = build_client()
    stop = False

    def _handle_signal(_signum: int, _frame: Any) -> None:
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, _handle_signal)
    signal.signal(signal.SIGTERM, _handle_signal)

    if not connect_client(client):
        return 1

    state_entered_at = time.monotonic()
    last_sensor_at = 0.0

    publish_state(client, sm)
    run_state_actions(sm.current)
    publish_sensors(client, sm)
    last_sensor_at = time.monotonic()

    try:
        while not stop:
            now = time.monotonic()

            if now - last_sensor_at >= SENSOR_INTERVAL_S:
                publish_sensors(client, sm)
                last_sensor_at = now

            if now - state_entered_at >= STATE_DWELL_S:
                sm.advance()
                publish_state(client, sm)
                run_state_actions(sm.current)
                publish_sensors(client, sm)
                last_sensor_at = now
                state_entered_at = now

            time.sleep(0.05)
    finally:
        client.publish(TOPIC_STATUS, "offline", qos=1, retain=True)
        client.loop_stop()
        client.disconnect()
        log.info("Soldier stopped in state=%s", sm.payload())

    return 0


if __name__ == "__main__":
    sys.exit(main())
