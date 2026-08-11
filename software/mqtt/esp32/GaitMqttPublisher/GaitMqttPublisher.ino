/*
 * ======================================================================================
 *
 * Project:     Eragon Bipedal Robot
 * Module:      Gait State MQTT Publisher (Reflex → Medulla)
 * Hardware:    ESP32-WROOM
 * Description:
 *      Publishes whether Eragon is "walking" or "standing" over WiFi via MQTT
 *      to the Raspberry Pi (Medulla) broker.
 *
 *      Serial commands (newline-terminated):
 *          WALK  / walking  → publish walking
 *          STAND / standing → publish standing
 *          STATUS           → print current state
 *
 * Dependencies:
 *      - WiFi (ESP32 Arduino core)
 *      - PubSubClient (Arduino library manager)
 *
 * Setup:
 *      1. Copy secrets.h.example → secrets.h and edit WiFi / broker values
 *      2. Flash this sketch to the ESP32
 *      3. On the Pi: run software/mqtt/pi/gait_subscriber.py
 *
 * ======================================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

// --- MQTT topics (must match Pi subscriber) ---
static const char *TOPIC_GAIT_STATE = "eragon/gait/state";
static const char *TOPIC_GAIT_STATUS = "eragon/gait/status";  // retained online/offline
static const char *CLIENT_ID = "eragon-esp32-reflex";

enum class GaitState : uint8_t {
  Standing = 0,
  Walking = 1,
};

static const char *gaitStateToString(GaitState state) {
  return state == GaitState::Walking ? "walking" : "standing";
}

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

GaitState currentState = GaitState::Standing;
unsigned long lastReconnectAttemptMs = 0;
unsigned long lastHeartbeatMs = 0;
const unsigned long RECONNECT_INTERVAL_MS = 5000;
const unsigned long HEARTBEAT_INTERVAL_MS = 10000;  // re-publish state periodically

void setGaitState(GaitState next, bool forcePublish = false);
bool ensureMqttConnected();
void publishGaitState(bool retained = true);
void handleSerialCommand(const String &line);

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WIFI] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("[WIFI] Connected  IP=");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println(">>> ERAGON | Gait MQTT Publisher (walking | standing) <<<");
  Serial.println("    Commands: WALK | STAND | STATUS");

  setupWiFi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setKeepAlive(30);
  mqtt.setBufferSize(256);

  // First connect + publish initial standing state
  ensureMqttConnected();
  setGaitState(GaitState::Standing, true);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Lost connection — reconnecting");
    setupWiFi();
  }

  if (!mqtt.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttemptMs >= RECONNECT_INTERVAL_MS) {
      lastReconnectAttemptMs = now;
      ensureMqttConnected();
    }
  } else {
    mqtt.loop();

    // Periodic heartbeat so the Pi always has a fresh gait sample
    unsigned long now = millis();
    if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
      lastHeartbeatMs = now;
      publishGaitState(true);
    }
  }

  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      handleSerialCommand(line);
    }
  }
}

bool ensureMqttConnected() {
  if (mqtt.connected()) {
    return true;
  }

  Serial.print("[MQTT] Connecting to ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  // Last will: if ESP32 drops, broker publishes offline for subscribers
  bool ok;
  if (strlen(MQTT_USER) > 0) {
    ok = mqtt.connect(CLIENT_ID, MQTT_USER, MQTT_PASSWORD,
                      TOPIC_GAIT_STATUS, 1, true, "offline");
  } else {
    ok = mqtt.connect(CLIENT_ID, TOPIC_GAIT_STATUS, 1, true, "offline");
  }

  if (ok) {
    Serial.println("[MQTT] Connected");
    mqtt.publish(TOPIC_GAIT_STATUS, "online", true);
    publishGaitState(true);
  } else {
    Serial.print("[MQTT] Failed  rc=");
    Serial.println(mqtt.state());
  }
  return ok;
}

void publishGaitState(bool retained) {
  if (!mqtt.connected()) {
    return;
  }
  const char *payload = gaitStateToString(currentState);
  bool ok = mqtt.publish(TOPIC_GAIT_STATE, payload, retained);
  Serial.printf("[MQTT] %s → %s (%s)\n",
                TOPIC_GAIT_STATE, payload, ok ? "ok" : "fail");
}

void setGaitState(GaitState next, bool forcePublish) {
  if (!forcePublish && next == currentState) {
    Serial.printf("[GAIT] Already %s\n", gaitStateToString(currentState));
    return;
  }
  currentState = next;
  Serial.printf("[GAIT] State → %s\n", gaitStateToString(currentState));
  publishGaitState(true);
  lastHeartbeatMs = millis();
}

void handleSerialCommand(const String &line) {
  if (line.equalsIgnoreCase("WALK") || line.equalsIgnoreCase("walking")) {
    setGaitState(GaitState::Walking);
  } else if (line.equalsIgnoreCase("STAND") || line.equalsIgnoreCase("standing")) {
    setGaitState(GaitState::Standing);
  } else if (line.equalsIgnoreCase("STATUS")) {
    Serial.printf("[GAIT] current=%s  wifi=%s  mqtt=%s\n",
                  gaitStateToString(currentState),
                  WiFi.status() == WL_CONNECTED ? "up" : "down",
                  mqtt.connected() ? "up" : "down");
  } else {
    Serial.println("[CMD] Unknown — use WALK | STAND | STATUS");
  }
}
