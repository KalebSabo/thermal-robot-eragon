/*
 * ======================================================================================
 *
 * Project:     Eragon Bipedal Robot
 * Module:      Remote Servo Control (BLE + Serial Bridge)
 * Author:      Kaleb Sabo
 * Date:        February 2026
 * Hardware:    ESP32-WROOM, Standard Servo (Pin 18)
 * Description: 
 *      - Asynchronous controller for Eragon's joint movement. 
 * Utilizes FreeRTOS for multi-core task distribution:
 *      - Core 0: High-priority Servo PWM & Kinematic smoothing.
 *      - Core 1: BLE Server management, Serial telemetry, and Heartbeat LED.
 * Dependencies: 
 *      - ESP32Servo, BLEDevice, BLEServer, FreeRTOS
 * 
 * ======================================================================================
 */


#include <Arduino.h>
#include <ESP32Servo.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- 1. GLOBAL HANDLES ---
SemaphoreHandle_t serialMutex;
QueueHandle_t servoQueue;
Servo myServo;

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
String rxValue;

// Standard Nordic UART Service UUIDs
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" 
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

const int servoPin = 18;
const int ledPin = 2;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; };
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

// --- CALLBACK CLASS ---
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      rxValue = pCharacteristic->getValue(); 
      
      if (rxValue.length() > 0) {
        rxValue.trim(); // Remove any stray newlines or spaces
        
        if (xSemaphoreTake(serialMutex, 0)) {
            Serial.print("[BLE] Received: ");
            Serial.println(rxValue);
            xSemaphoreGive(serialMutex);
        }

        // Logic to bridge BLE to the Servo Queue
        int val;
        if (rxValue.equalsIgnoreCase("STOP")) {
            val = -1;
        } else {
            val = rxValue.toInt();
            // Basic validation: ignore if toInt fails (returns 0) and input wasn't "0"
            if (val == 0 && rxValue != "0") return; 
        }

        // Send the value to the same queue the Serial task uses
        // This allows the Servo task to react to either source!
        xQueueSend(servoQueue, &val, 0); 
      }
    }
};


// --- TASK CONFIGURATION ---
// Distributed across ESP32 Dual Cores to prevent BLE interrupts from jittering servos.
void taskServo(void *pvParameters);  // PRO: Handles smooth motion on Core 0
void taskSerial(void *pvParameters); // CON: Handles user input on Core 1
void taskBlink(void *pvParameters);  // CON: System status indicator on Core 1

void setup() {
    Serial.begin(115200);

    BLEDevice::init("ESP32_UART_Bridge");


    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                        );
    pTxCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                                            CHARACTERISTIC_UUID_RX,
                                            BLECharacteristic::PROPERTY_WRITE
                                            );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("Waiting for a client connection to notify...");



    // Create synchronization tools
    serialMutex = xSemaphoreCreateMutex();
    servoQueue = xQueueCreate(10, sizeof(int));

    configASSERT(serialMutex != NULL);
    configASSERT(servoQueue != NULL);

    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Launch the "Triple Threat" of tasks
    xTaskCreatePinnedToCore(taskServo,  "Servo",  4096, NULL, 2, NULL, 0); // Core 0
    xTaskCreatePinnedToCore(taskSerial, "Reader", 3072, NULL, 1, NULL, 1); // Core 1
    xTaskCreatePinnedToCore(taskBlink,  "LED",    2048, NULL, 1, NULL, 1); // Core 1

    if (xSemaphoreTake(serialMutex, portMAX_DELAY)) {
        Serial.println(">>> SYSTEM ONLINE | Enter Angle (0-180) or 'STOP' <<<");
        xSemaphoreGive(serialMutex);
    }
}

void loop() { 
    if (deviceConnected) {
        // Send current status to your phone every 2 seconds
        pTxCharacteristic->setValue("System Heartbeat: OK");
        pTxCharacteristic->notify();
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    }

// --- 3. SERVO TASK WITH TELEMETRY ---
void taskServo(void *pvParameters) {
    myServo.attach(servoPin);
    float currentAngle = 90.0;
    int targetAngle = 90;
    bool isEnabled = true;

    for (;;) {
        // Check for new commands
        if (xQueueReceive(servoQueue, &targetAngle, 0)) {
            if (targetAngle == -1) {
                isEnabled = false;
                myServo.detach();
            } else {
                if (!isEnabled) { myServo.attach(servoPin); isEnabled = true; }
                targetAngle = constrain(targetAngle, 0, 180);
            }
        }

        // Movement & Telemetry Logic
        if (isEnabled && abs(targetAngle - currentAngle) > 0.05) {
            currentAngle += (targetAngle - currentAngle) * 0.05;
            myServo.write((int)currentAngle);

            // Mutex-protected Serial logging
            if (xSemaphoreTake(serialMutex, 0)) {
                Serial.printf("[SERVO] Core:%d | Pos: %.1f | Goal: %d\n", 
                              xPortGetCoreID(), currentAngle, targetAngle);
                xSemaphoreGive(serialMutex);
            }
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

// --- 4. SERIAL LISTENER ---
void taskSerial(void *pvParameters) {
    for (;;) {
        if (Serial.available() > 0) {
            String input = Serial.readStringUntil('\n');
            input.trim();

            int val;
            if (input.equalsIgnoreCase("STOP")) {
                val = -1;
            } else {
                val = input.toInt();
                if (val == 0 && input != "0") continue; // Ignore garbage text
            }

            xQueueSend(servoQueue, &val, portMAX_DELAY);
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// --- 5. HEARTBEAT LED ---
void taskBlink(void *pvParameters) {
    pinMode(ledPin, OUTPUT);
    for (;;) {
        digitalWrite(ledPin, !digitalRead(ledPin)); // Toggle LED
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}