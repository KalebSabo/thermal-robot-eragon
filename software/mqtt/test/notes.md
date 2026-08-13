# Eragon Test — ROS2-Micro State Machine Template (Python)

Lightweight develop-and-test template for Eragon’s **Reflex → Medulla** split. A three-state command loop runs on the “soldier” side, publishes mock sensor data over MQTT, and a “commander” module on the Pi subscribes and analyzes mission status.

This folder is intentionally **Python-only** (no ROS2/micro-ROS toolchain) so you can iterate on state logic and MQTT contracts before porting to ESP32 firmware.

## Layout

```
software/mqtt/test/
  state_machine.py      Three-state FSM (calibration → forward → stop)
  sensors.py            Mock servo, distance, and camera payloads per state
  topics.py             Shared MQTT topic names
  soldier_fsm.py        Reflex publisher — loops states, publishes telemetry
  commander_server.py   Medulla subscriber — validates and analyzes feedback
  requirements.txt
  notes.md
```

## Command loop

Matches the Eragon Ep. 1 outgoing command convention:

| State | MQTT payload | Mock behavior |
|-------|--------------|---------------|
| Calibration/Stand | `calibration` | Neutral servo pose, zero velocity |
| Forward | `forward` | Gait-like servo jitter, decreasing distance |
| Stop | `stop` | Hold pose, zero velocity |

```
  calibration ──► forward ──► stop ──► calibration …
```

## MQTT topics

| Topic | Publisher | Payload |
|-------|-----------|---------|
| `eragon/test/state` | soldier | `calibration` \| `forward` \| `stop` (retained) |
| `eragon/test/sensors` | soldier | JSON: servos, distance, velocity, camera |
| `eragon/test/status` | soldier | `online` \| `offline` |

Production gait topics (`eragon/gait/*`) are unchanged; this template uses the `eragon/test/*` namespace.

## Quick start (no hardware)

```bash
# Terminal A — broker (if not already running)
sudo apt install -y mosquitto mosquitto-clients
sudo systemctl enable --now mosquitto

# Terminal B — commander (Medulla)
cd software/mqtt/test
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python3 commander_server.py

# Terminal C — soldier (Reflex FSM)
cd software/mqtt/test
source .venv/bin/activate
python3 soldier_fsm.py
```

Tune loop timing:

```bash
STATE_DWELL_S=5 SENSOR_INTERVAL_S=1 python3 soldier_fsm.py
```

## Commander output

The commander prints JSON lines suitable for piping or logging:

- **State messages** — current command and mission phase (`prepare`, `advance`, `halt`)
- **Sensor analysis** — derived `mission_status` (`moving`, `holding`, `obstacle_detected`, `wall_reached`)

## Related

- Gait MQTT bridge: `software/mqtt/notes.md`
- State machine concept: `docs/state-machine.md`
- Eragon architecture: `README.md`
