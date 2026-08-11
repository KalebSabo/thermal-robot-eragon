/*
 * ======================================================================================
 *
 * Project:     Eragon Bipedal Robot
 * Module:      Single Servo Calibration
 * Author:      Kaleb Sabo
 * Date:        August 2026
 * Hardware:    ESP32-WROOM, Standard Servo (Pin 18)
 * Description:
 *      Interactive serial tool to calibrate one servo before joint assembly.
 *      Use it to find center, safe angle limits, and usable pulse-width range.
 * Dependencies:
 *      - ESP32Servo
 *
 * ======================================================================================
 *
 * Serial commands (115200 baud, newline-terminated):
 *      help              — show this menu
 *      status            — print current angle / pulse / saved limits
 *      <0-180>           — move to angle in degrees
 *      us <500-2500>     — move by pulse width in microseconds
 *      center            — move to 90°
 *      min               — save current position as mechanical MIN
 *      max               — save current position as mechanical MAX
 *      sweep             — slow sweep between saved MIN and MAX
 *      step <+/-N>       — nudge angle by N degrees (e.g. step +5)
 *      stop              — detach servo (release holding torque)
 *      attach            — re-attach servo at last angle
 *
 * ======================================================================================
 */

#include <Arduino.h>
#include <ESP32Servo.h>

const int SERVO_PIN = 18;
const int LED_PIN = 2;

// Typical hobby-servo pulse range; adjust if your servo datasheet differs.
const int PULSE_MIN_US = 500;
const int PULSE_MAX_US = 2500;

Servo servo;
bool servoAttached = false;

int currentAngle = 90;
int pulseUs = 1500;

// Mechanical limits discovered during calibration (angle degrees).
int limitMin = 0;
int limitMax = 180;
bool limitMinSet = false;
bool limitMaxSet = false;

void printHelp() {
    Serial.println();
    Serial.println(F("=== Eragon Single-Servo Calibration ==="));
    Serial.println(F("  help              show commands"));
    Serial.println(F("  status            current angle / pulse / limits"));
    Serial.println(F("  <0-180>           move to angle (degrees)"));
    Serial.println(F("  us <500-2500>     move by pulse width (us)"));
    Serial.println(F("  center            go to 90 deg"));
    Serial.println(F("  min / max         save current pos as limit"));
    Serial.println(F("  sweep             sweep between saved limits"));
    Serial.println(F("  step <+/-N>       nudge angle by N degrees"));
    Serial.println(F("  stop / attach     detach or re-attach servo"));
    Serial.println();
}

void printStatus() {
    Serial.printf(
        "[STATUS] angle=%d deg | pulse=%d us | attached=%s | limits=[%s%d .. %s%d]\n",
        currentAngle,
        pulseUs,
        servoAttached ? "yes" : "no",
        limitMinSet ? "" : "?",
        limitMin,
        limitMaxSet ? "" : "?",
        limitMax
    );
}

int angleToPulse(int angle) {
    angle = constrain(angle, 0, 180);
    return map(angle, 0, 180, PULSE_MIN_US, PULSE_MAX_US);
}

int pulseToAngle(int us) {
    us = constrain(us, PULSE_MIN_US, PULSE_MAX_US);
    return map(us, PULSE_MIN_US, PULSE_MAX_US, 0, 180);
}

void ensureAttached() {
    if (!servoAttached) {
        servo.attach(SERVO_PIN, PULSE_MIN_US, PULSE_MAX_US);
        servoAttached = true;
        digitalWrite(LED_PIN, HIGH);
        Serial.println(F("[SERVO] attached"));
    }
}

void moveToAngle(int angle) {
    angle = constrain(angle, 0, 180);
    ensureAttached();
    currentAngle = angle;
    pulseUs = angleToPulse(angle);
    servo.write(currentAngle);
    Serial.printf("[MOVE] angle=%d deg (~%d us)\n", currentAngle, pulseUs);
}

void moveToPulse(int us) {
    us = constrain(us, PULSE_MIN_US, PULSE_MAX_US);
    ensureAttached();
    pulseUs = us;
    currentAngle = pulseToAngle(us);
    servo.writeMicroseconds(pulseUs);
    Serial.printf("[MOVE] pulse=%d us (~%d deg)\n", pulseUs, currentAngle);
}

void detachServo() {
    if (servoAttached) {
        servo.detach();
        servoAttached = false;
        digitalWrite(LED_PIN, LOW);
        Serial.println(F("[SERVO] detached (holding torque released)"));
    } else {
        Serial.println(F("[SERVO] already detached"));
    }
}

void runSweep() {
    int startAngle = limitMinSet ? limitMin : 0;
    int endAngle = limitMaxSet ? limitMax : 180;

    if (startAngle > endAngle) {
        int tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    Serial.printf("[SWEEP] %d -> %d -> %d (slow)\n", startAngle, endAngle, startAngle);
    ensureAttached();

    for (int a = startAngle; a <= endAngle; a++) {
        currentAngle = a;
        pulseUs = angleToPulse(a);
        servo.write(a);
        delay(20);
        if (Serial.available() > 0) {
            Serial.println(F("[SWEEP] aborted"));
            return;
        }
    }
    for (int a = endAngle; a >= startAngle; a--) {
        currentAngle = a;
        pulseUs = angleToPulse(a);
        servo.write(a);
        delay(20);
        if (Serial.available() > 0) {
            Serial.println(F("[SWEEP] aborted"));
            return;
        }
    }

    Serial.println(F("[SWEEP] done"));
    printStatus();
}

void handleCommand(String input) {
    input.trim();
    if (input.length() == 0) {
        return;
    }

    if (input.equalsIgnoreCase("help") || input.equalsIgnoreCase("?")) {
        printHelp();
        return;
    }

    if (input.equalsIgnoreCase("status")) {
        printStatus();
        return;
    }

    if (input.equalsIgnoreCase("center")) {
        moveToAngle(90);
        return;
    }

    if (input.equalsIgnoreCase("min")) {
        limitMin = currentAngle;
        limitMinSet = true;
        Serial.printf("[CAL] MIN limit saved at %d deg\n", limitMin);
        printStatus();
        return;
    }

    if (input.equalsIgnoreCase("max")) {
        limitMax = currentAngle;
        limitMaxSet = true;
        Serial.printf("[CAL] MAX limit saved at %d deg\n", limitMax);
        printStatus();
        return;
    }

    if (input.equalsIgnoreCase("sweep")) {
        runSweep();
        return;
    }

    if (input.equalsIgnoreCase("stop")) {
        detachServo();
        return;
    }

    if (input.equalsIgnoreCase("attach")) {
        ensureAttached();
        moveToAngle(currentAngle);
        return;
    }

    if (input.startsWith("us ") || input.startsWith("US ")) {
        int us = input.substring(3).toInt();
        if (us < PULSE_MIN_US || us > PULSE_MAX_US) {
            Serial.printf("[ERR] pulse must be %d-%d us\n", PULSE_MIN_US, PULSE_MAX_US);
            return;
        }
        moveToPulse(us);
        return;
    }

    if (input.startsWith("step ") || input.startsWith("STEP ")) {
        int delta = input.substring(5).toInt();
        if (delta == 0 && input.substring(5) != "0") {
            Serial.println(F("[ERR] usage: step <+/-N>"));
            return;
        }
        moveToAngle(currentAngle + delta);
        return;
    }

    // Bare integer => angle in degrees
    bool looksNumeric = true;
    for (unsigned int i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        if (i == 0 && (c == '+' || c == '-')) {
            continue;
        }
        if (!isDigit(c)) {
            looksNumeric = false;
            break;
        }
    }

    if (looksNumeric) {
        int angle = input.toInt();
        if (angle < 0 || angle > 180) {
            Serial.println(F("[ERR] angle must be 0-180"));
            return;
        }
        moveToAngle(angle);
        return;
    }

    Serial.print(F("[ERR] unknown command: "));
    Serial.println(input);
    Serial.println(F("Type 'help' for commands."));
}

void setup() {
    Serial.begin(115200);
    delay(300);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Allocate ESP32 PWM timers used by ESP32Servo.
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    servo.setPeriodHertz(50);
    ensureAttached();
    moveToAngle(90);

    printHelp();
    printStatus();
    Serial.println(F("Ready. Move the horn by hand only while STOP'd, then attach + center."));
}

void loop() {
    if (Serial.available() > 0) {
        String line = Serial.readStringUntil('\n');
        handleCommand(line);
    }
}
