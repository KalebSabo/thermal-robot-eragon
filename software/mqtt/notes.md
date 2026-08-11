# Eragon MQTT — Gait State (walking / standing)

Medulla (Pi) ↔ Reflex (ESP32) bridge over WiFi using MQTT.

## Layout

```
software/mqtt/
  esp32/GaitMqttPublisher/   ESP32 publisher (Arduino)
  pi/                        Raspberry Pi subscriber (Python)
```

## Topics

| Topic | Publisher | Payload | Notes |
|-------|-----------|---------|-------|
| `eragon/gait/state` | ESP32 | `walking` or `standing` | Retained; heartbeat every 10 s |
| `eragon/gait/status` | ESP32 / broker LWT | `online` or `offline` | Link health |

## Pi setup (broker + subscriber)

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients
sudo systemctl enable --now mosquitto

# Allow LAN clients (ESP32) if needed — edit /etc/mosquitto/mosquitto.conf
# listener 1883
# allow_anonymous true

cd software/mqtt/pi
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python3 gait_subscriber.py
```

Override broker host if the script is not on the same machine:

```bash
MQTT_BROKER=192.168.1.100 python3 gait_subscriber.py
```

## ESP32 setup (publisher)

1. Install **PubSubClient** in Arduino Library Manager.
2. Copy `secrets.h.example` → `secrets.h` and set WiFi SSID/password plus the Pi’s LAN IP.
3. Flash `GaitMqttPublisher.ino`.
4. Serial Monitor (115200): send `WALK` or `STAND` to publish gait state.

## Quick test without hardware

On the Pi (with Mosquitto running):

```bash
# Terminal A
python3 gait_subscriber.py

# Terminal B
mosquitto_pub -t eragon/gait/state -m walking -r
mosquitto_pub -t eragon/gait/state -m standing -r
```
