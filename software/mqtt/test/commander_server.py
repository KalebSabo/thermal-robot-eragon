#!/usr/bin/env python3
"""
Eragon test Medulla — commander/server module for the three-state FSM template.

Subscribes to mock Reflex state and sensor MQTT traffic, validates payloads,
and prints mission-style feedback (analyze status → assign mission phase).

Usage:
  python3 commander_server.py
  MQTT_BROKER=127.0.0.1 python3 commander_server.py
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

from topics import (
    CLIENT_ID_COMMANDER,
    TOPIC_SENSORS,
    TOPIC_STATE,
    TOPIC_STATUS,
    VALID_STATES,
)

BROKER = os.environ.get("MQTT_BROKER", "127.0.0.1")
PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_USER = os.environ.get("MQTT_USER", "")
MQTT_PASSWORD = os.environ.get("MQTT_PASSWORD", "")

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
log = logging.getLogger("eragon.test.commander")


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def mission_phase_for_state(state: str) -> str:
    if state == "calibration":
        return "prepare"
    if state == "forward":
        return "advance"
    return "halt"


def analyze_sensors(record: dict[str, Any]) -> dict[str, Any]:
    distance = float(record.get("distance_cm", 0))
    velocity = float(record.get("velocity_cm_s", 0))
    camera = record.get("camera") or {}
    obstacle = bool(camera.get("obstacle_ahead"))

    if distance < 10.0:
        mission_status = "wall_reached"
    elif obstacle:
        mission_status = "obstacle_detected"
    elif velocity > 0.5:
        mission_status = "moving"
    else:
        mission_status = "holding"

    return {
        "timestamp": utc_now_iso(),
        "state": record.get("state"),
        "mission_phase": mission_phase_for_state(str(record.get("state", ""))),
        "mission_status": mission_status,
        "distance_cm": distance,
        "velocity_cm_s": velocity,
        "obstacle_ahead": obstacle,
    }


def build_client() -> mqtt.Client:
    try:
        client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id=CLIENT_ID_COMMANDER,
            protocol=mqtt.MQTTv311,
        )
    except (AttributeError, TypeError):
        client = mqtt.Client(client_id=CLIENT_ID_COMMANDER, protocol=mqtt.MQTTv311)

    if MQTT_USER:
        client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
    return client


def on_connect(
    client: mqtt.Client,
    _userdata: Any,
    _flags: dict[str, Any],
    reason_code: int,
    _properties: Any = None,
) -> None:
    if reason_code != 0:
        log.error("MQTT connect failed: reason_code=%s", reason_code)
        return
    log.info("Connected to broker %s:%s as %s", BROKER, PORT, CLIENT_ID_COMMANDER)
    client.subscribe(
        [
            (TOPIC_STATE, 1),
            (TOPIC_SENSORS, 0),
            (TOPIC_STATUS, 1),
        ]
    )
    log.info("Subscribed to %s, %s, %s", TOPIC_STATE, TOPIC_SENSORS, TOPIC_STATUS)


def on_disconnect(
    _client: mqtt.Client,
    _userdata: Any,
    reason_code: int,
    _properties: Any = None,
) -> None:
    log.warning("Disconnected from broker (reason_code=%s)", reason_code)


def on_message(_client: mqtt.Client, _userdata: Any, msg: mqtt.MQTTMessage) -> None:
    payload = msg.payload.decode("utf-8", errors="replace").strip()
    retained = " retained" if msg.retain else ""

    if msg.topic == TOPIC_STATUS:
        log.info("Reflex link%s: %s", retained, payload)
        return

    if msg.topic == TOPIC_STATE:
        state = payload.lower()
        if state not in VALID_STATES:
            log.warning("Unknown state payload on %s: %r", msg.topic, payload)
            return
        record = {
            "timestamp": utc_now_iso(),
            "topic": msg.topic,
            "state": state,
            "mission_phase": mission_phase_for_state(state),
            "qos": msg.qos,
            "retained": bool(msg.retain),
        }
        log.info("COMMAND %s (phase=%s)", state.upper(), record["mission_phase"])
        print(json.dumps(record), flush=True)
        return

    if msg.topic == TOPIC_SENSORS:
        try:
            record = json.loads(payload)
        except json.JSONDecodeError:
            log.warning("Invalid JSON on %s: %r", msg.topic, payload[:120])
            return

        analysis = analyze_sensors(record)
        log.info(
            "SENSORS phase=%s status=%s dist=%scm",
            analysis["mission_phase"],
            analysis["mission_status"],
            analysis["distance_cm"],
        )
        print(json.dumps(analysis), flush=True)
        return

    log.debug("Ignored topic %s payload=%r", msg.topic, payload)


def main() -> int:
    log.info("Eragon test commander starting")
    log.info("Broker=%s:%s  expecting states: calibration | forward | stop", BROKER, PORT)

    client = build_client()
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    stop = False

    def _handle_signal(_signum: int, _frame: Any) -> None:
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, _handle_signal)
    signal.signal(signal.SIGTERM, _handle_signal)

    try:
        client.connect(BROKER, PORT, keepalive=60)
    except OSError as exc:
        log.error("Cannot reach MQTT broker at %s:%s — %s", BROKER, PORT, exc)
        return 1

    client.loop_start()
    try:
        while not stop:
            time.sleep(0.25)
    finally:
        client.loop_stop()
        client.disconnect()
        log.info("Commander stopped")

    return 0


if __name__ == "__main__":
    sys.exit(main())
