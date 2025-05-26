// forza_telemetry_arduino.ino
/*
  ESP32 Sketch to Receive Processed G-Forces from PC via USB Serial and Control Motors

  This sketch:
  1. Reads a comma-separated string of G-force values (longitudinal, lateral, vertical)
     from the USB serial port, sent by a companion PC application.
  2. Parses these G-force values.
  3. (Example) Maps these G-forces to PWM signals for hypothetical motor control.

  Setup Instructions:
  1. Hardware: Arduino Mega 2560 development board connected to PC via USB.
  2. Arduino IDE: Standard Arduino IDE setup.
  3. PC Application: Use the provided Python script (`forza_to_arduino_pc_app.py`)
     or a similar application that calculates G-forces from Forza telemetry and
     sends them to the Arduino's serial port in the format "long,lat,vert\\n".
  4. Upload this sketch to your Arduino Mega 2560.
  5. Open the Serial Monitor in the Arduino IDE (baud rate 115200) to see logs
     and the G-force data received from the PC.
  6. Run the PC forwarding application.
  7. Start racing in Forza Horizon!
*/

#include <Arduino.h>
// #include <math.h> // math.h is implicitly included by Arduino.h

// --- Configuration ---
const int BAUD_RATE = 115200;

// Example Motor Control Pins (Update to your actual motor driver pins for Arduino Mega 2560)
// These are just placeholders. You'll need a motor driver (e.g., L298N, TB6612FNG).
// Ensure these pins are PWM-capable on the Mega 2560 if used with analogWrite
// (Mega PWM pins: 2-13, 44-46)
const int MOTOR_LONG_PIN_FWD = 9;  // Example: PWM Pin for longitudinal G - forward/acceleration
const int MOTOR_LONG_PIN_REV = 10; // Example: PWM Pin for longitudinal G - reverse/braking
const int MOTOR_LAT_PIN_LEFT = 5;  // Example: PWM Pin for lateral G - left
const int MOTOR_LAT_PIN_RIGHT = 6; // Example: PWM Pin for lateral G - right
// Vertical G might be more complex (e.g., controlling a platform height)
// For simplicity, we'll just print it.

// PWM Properties
// const int PWM_FREQ = 5000; // Not directly set for analogWrite on Mega, uses default
const int PWM_RESOLUTION_BITS = 8; // analogWrite on Mega is effectively 8-bit
const int MAX_PWM_VALUE = (1 << PWM_RESOLUTION_BITS) - 1; // 255

// PWM Channels (Not used in the same way for Mega's analogWrite)
// const int PWM_CHAN_LONG_FWD = 0; // Removed
// const int PWM_CHAN_LONG_REV = 1; // Removed
// const int PWM_CHAN_LAT_LEFT = 2; // Removed
// const int PWM_CHAN_LAT_RIGHT = 3; // Removed


char serialBuffer[100]; // Buffer to store incoming serial data
int serialBufferPos = 0;

unsigned long lastDataReceivedTime = 0;
unsigned long lastStatusPrintTime = 0;

void setup() {
    Serial.begin(BAUD_RATE);
    Serial.println("\\n===============================================");
    Serial.println("🏎️ Arduino Mega G-Force Motor Controller (USB Serial)");
    Serial.println("===============================================");
    Serial.println("[SERIAL] Arduino Mega ready. Waiting for G-Force data from PC (format: long,lat,vert\\n)");

    // --- Setup Motor Pins ---
    // Longitudinal Motor
    pinMode(MOTOR_LONG_PIN_FWD, OUTPUT);
    pinMode(MOTOR_LONG_PIN_REV, OUTPUT);
    // ledcSetup(PWM_CHAN_LONG_FWD, PWM_FREQ, PWM_RESOLUTION_BITS); // Removed
    // ledcSetup(PWM_CHAN_LONG_REV, PWM_FREQ, PWM_RESOLUTION_BITS); // Removed
    // ledcAttachPin(MOTOR_LONG_PIN_FWD, PWM_CHAN_LONG_FWD); // Removed
    // ledcAttachPin(MOTOR_LONG_PIN_REV, PWM_CHAN_LONG_REV); // Removed

    // Lateral Motor
    pinMode(MOTOR_LAT_PIN_LEFT, OUTPUT);
    pinMode(MOTOR_LAT_PIN_RIGHT, OUTPUT);
    // ledcSetup(PWM_CHAN_LAT_LEFT, PWM_FREQ, PWM_RESOLUTION_BITS); // Removed
    // ledcSetup(PWM_CHAN_LAT_RIGHT, PWM_FREQ, PWM_RESOLUTION_BITS); // Removed
    // ledcAttachPin(MOTOR_LAT_PIN_LEFT, PWM_CHAN_LAT_LEFT); // Removed
    // ledcAttachPin(MOTOR_LAT_PIN_RIGHT, PWM_CHAN_LAT_RIGHT); // Removed

    Serial.println("[MOTORS] Example motor pins configured for Arduino Mega.");
    Serial.println("[INFO] Ensure you have a suitable motor driver connected.");
    Serial.println("[INFO] Ensure motor pins are PWM capable (2-13, 44-46 on Mega).");
    
    // Ensure motors are off initially using analogWrite for PWM pins
    analogWrite(MOTOR_LONG_PIN_FWD, 0);
    analogWrite(MOTOR_LONG_PIN_REV, 0);
    analogWrite(MOTOR_LAT_PIN_LEFT, 0);
    analogWrite(MOTOR_LAT_PIN_RIGHT, 0);
    // digitalWrite(MOTOR_LONG_PIN_FWD, LOW); // Replaced by analogWrite
    // digitalWrite(MOTOR_LONG_PIN_REV, LOW); // Replaced by analogWrite
    // digitalWrite(MOTOR_LAT_PIN_LEFT, LOW); // Replaced by analogWrite
    // digitalWrite(MOTOR_LAT_PIN_RIGHT, LOW); // Replaced by analogWrite
    // ledcWrite(PWM_CHAN_LONG_FWD, 0); // Removed
    // ledcWrite(PWM_CHAN_LONG_REV, 0); // Removed
    // ledcWrite(PWM_CHAN_LAT_LEFT, 0); // Removed
    // ledcWrite(PWM_CHAN_LAT_RIGHT, 0); // Removed

    lastDataReceivedTime = millis();
}

void processGForceData(float gLong, float gLat, float gVert) {
    // --- Example Motor Control Logic ---
    // This is a very basic example. Real motor control would be more sophisticated,
    // potentially involving PID controllers, dead zones, and careful mapping of G-forces.

    // Longitudinal G-Force (Acceleration/Braking)
    // gLong > 0 typically means braking/deceleration (force pushing you forward)
    // gLong < 0 typically means acceleration (force pushing you back)
    int pwmLongFwd = 0;
    int pwmLongRev = 0;
    if (gLong > 0.1) { // Braking - adjust threshold as needed
        pwmLongFwd = map(gLong * 100, 10, 300, 0, MAX_PWM_VALUE); // Scale 0.1G to 3G
    } else if (gLong < -0.1) { // Acceleration
        pwmLongRev = map(abs(gLong) * 100, 10, 300, 0, MAX_PWM_VALUE); // Scale 0.1G to 3G
    }
    // ledcWrite(PWM_CHAN_LONG_FWD, constrain(pwmLongFwd, 0, MAX_PWM_VALUE)); // Removed
    // ledcWrite(PWM_CHAN_LONG_REV, constrain(pwmLongRev, 0, MAX_PWM_VALUE)); // Removed
    analogWrite(MOTOR_LONG_PIN_FWD, constrain(pwmLongFwd, 0, MAX_PWM_VALUE));
    analogWrite(MOTOR_LONG_PIN_REV, constrain(pwmLongRev, 0, MAX_PWM_VALUE));

    // Lateral G-Force (Cornering)
    // gLat > 0 typically means force to the left (right turn)
    // gLat < 0 typically means force to the right (left turn)
    int pwmLatLeft = 0;
    int pwmLatRight = 0;
    if (gLat > 0.1) { // Turning Right (force pushes left)
        pwmLatLeft = map(gLat * 100, 10, 300, 0, MAX_PWM_VALUE);
    } else if (gLat < -0.1) { // Turning Left (force pushes right)
        pwmLatRight = map(abs(gLat) * 100, 10, 300, 0, MAX_PWM_VALUE);
    }
    // ledcWrite(PWM_CHAN_LAT_LEFT, constrain(pwmLatLeft, 0, MAX_PWM_VALUE)); // Removed
    // ledcWrite(PWM_CHAN_LAT_RIGHT, constrain(pwmLatRight, 0, MAX_PWM_VALUE)); // Removed
    analogWrite(MOTOR_LAT_PIN_LEFT, constrain(pwmLatLeft, 0, MAX_PWM_VALUE));
    analogWrite(MOTOR_LAT_PIN_RIGHT, constrain(pwmLatRight, 0, MAX_PWM_VALUE));


    // Print status periodically
    if (millis() - lastStatusPrintTime > 1000) { // Print every 1 second
        Serial.print("Arduino Mega RX GForces: ");
        Serial.print("Lng: "); Serial.print(gLong, 2);
        Serial.print(", Lat: "); Serial.print(gLat, 2);
        Serial.print(", Vrt: "); Serial.print(gVert, 2);
        Serial.print(" | PWM LngF: "); Serial.print(pwmLongFwd);
        Serial.print(", LngR: "); Serial.print(pwmLongRev);
        Serial.print(", LatL: "); Serial.print(pwmLatLeft);
        Serial.print(", LatR: "); Serial.println(pwmLatRight);
        lastStatusPrintTime = millis();
    }
}

void loop() {
    while (Serial.available() > 0) {
        char incomingChar = Serial.read();
        lastDataReceivedTime = millis();

        if (incomingChar == '\\n' || incomingChar == '\\r') { // End of line
            if (serialBufferPos > 0) { // We have some data
                serialBuffer[serialBufferPos] = '\\0'; // Null-terminate

                // Parse the data: "long,lat,vert"
                float gLong = 0.0, gLat = 0.0, gVert = 0.0;
                int numValues = sscanf(serialBuffer, "%f,%f,%f", &gLong, &gLat, &gVert);

                if (numValues == 3) {
                    processGForceData(gLong, gLat, gVert);
                } else {
                    Serial.print("[WARN] Could not parse G-force data: ");
                    Serial.println(serialBuffer);
                }
                serialBufferPos = 0; // Reset buffer
            }
        } else {
            if (serialBufferPos < sizeof(serialBuffer) - 1) {
                serialBuffer[serialBufferPos++] = incomingChar;
            } else {
                // Buffer overflow, discard and reset
                Serial.println("[ERROR] Serial buffer overflow. Discarding data.");
                serialBufferPos = 0;
            }
        }
    }

    // Timeout check: if no data received for a while, stop motors
    if (millis() - lastDataReceivedTime > 1000) { // 1 second timeout
        if (millis() - lastStatusPrintTime > 2000) { // Avoid spamming if already printing status
             Serial.println("[TIMEOUT] No data from PC. Stopping motors.");
             lastStatusPrintTime = millis(); // Reset status time to print this once
        }
        processGForceData(0.0, 0.0, 1.0); // Send neutral G-forces to stop motors
        lastDataReceivedTime = millis(); // Reset timeout timer to avoid continuous messages after first one
    }
    delay(1); // Small delay
}