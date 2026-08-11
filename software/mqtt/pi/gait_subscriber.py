#!/usr/bin/env python3
"""
Eragon Medulla — MQTT gait-state subscriber (Raspberry Pi)

Subscribes to gait telemetry published by the ESP32 Reflex layer over WiFi
and logs whether the robot is "walking" or "standing".

Topics (must match ESP32 publisher):
  eragon/gait/state   → payload: walking | standing
  eragon/gait/status  → payload: online | offline  (ESP32 LWT)

Broker:
  Install Mosquitto on the Pi, then run this script on the same host
  (or any LAN machine that can reach the broker).

  sudo apt update && sudo apt install -y mosquitto mosquitto-clients
  sudo systemctl enable --now mosquitto

Usage:
  python3 gait_subscriber.py
  MQTT_BROKER=192.168.1.100 python3 gait_subscriber.py
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

TOPIC_GAIT_STATE = "eragon/gait/state"
TOPIC_GAIT_STATUS = "eragon/gait/status"
VALID_STATES = frozenset({"walking", "standing"})

BROKER = os.environ.get("MQTT_BROKER", "127.0.0.1")
PORT = int(os.environ.get("MQTT_PORT", "1883"))
CLIENT_ID = os.environ.get("MQTT_CLIENT_ID", "eragon-pi-medulla")
MQTT_USER = os.environ.get("MQTT_USER", "")
MQTT_PASSWORD = os.environ.get("MQTT_PASSWORD", "")

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
log = logging.getLogger("eragon.gait")


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


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
    log.info("Connected to broker %s:%s as %s", BROKER, PORT, CLIENT_ID)
    client.subscribe([(TOPIC_GAIT_STATE, 1), (TOPIC_GAIT_STATUS, 1)])
    log.info("Subscribed to %s and %s", TOPIC_GAIT_STATE, TOPIC_GAIT_STATUS)


def on_disconnect(
    _client: mqtt.Client,
    _userdata: Any,
    reason_code: int,
    _properties: Any = None,
) -> None:
    log.warning("Disconnected from broker (reason_code=%s)", reason_code)


def on_message(_client: mqtt.Client, _userdata: Any, msg: mqtt.MQTTMessage) -> None:
    payload = msg.payload.decode("utf-8", errors="replace").strip().lower()
    retained = " retained" if msg.retain else ""

    if msg.topic == TOPIC_GAIT_STATUS:
        log.info("ESP32 link%s: %s", retained, payload)
        return

    if msg.topic == TOPIC_GAIT_STATE:
        if payload not in VALID_STATES:
            log.warning("Unknown gait payload on %s: %r", msg.topic, payload)
            return

        record = {
            "timestamp": utc_now_iso(),
            "topic": msg.topic,
            "state": payload,
            "qos": msg.qos,
            "retained": bool(msg.retain),
        }
        # Human-readable log + machine-friendly JSON line for later piping
        log.info("GAIT %s", payload.upper())
        print(json.dumps(record), flush=True)
        return

    log.debug("Ignored topic %s payload=%r", msg.topic, payload)


def build_client() -> mqtt.Client:
    # Compatible with both paho-mqtt v1 and v2 callback APIs
    try:
        client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id=CLIENT_ID,
            protocol=mqtt.MQTTv311,
        )
    except (AttributeError, TypeError):
        client = mqtt.Client(client_id=CLIENT_ID, protocol=mqtt.MQTTv311)

    if MQTT_USER:
        client.username_pw_set(MQTT_USER, MQTT_PASSWORD)

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    return client


def main() -> int:
    log.info("Eragon Medulla gait subscriber starting")
    log.info("Broker=%s:%s  expecting states: walking | standing", BROKER, PORT)

    client = build_client()
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
        log.error(
            "Is Mosquitto running?  sudo systemctl status mosquitto"
        )
        return 1

    client.loop_start()
    try:
        while not stop:
            time.sleep(0.25)
    finally:
        client.loop_stop()
        client.disconnect()
        log.info("Subscriber stopped")

    return 0


if __name__ == "__main__":
    sys.exit(main())
