#include <Arduino.h>

#include "fns3devkit.hpp"
long samplesTaken = 0;  // Counter for calculating the Hz or read rate
long unblockedValue;    // Average IR at power up
long startTime;         // Used to calculate measurement rate

void setup() {
    Serial.begin(115200);
    prox_sensor_initialize();

    // Setup to sense up to 18 inches, max LED brightness
    byte ledBrightness = 0xFF;  // Options: 0=Off to 255=50mA
    byte sampleAverage = 4;     // Options: 1, 2, 4, 8, 16, 32
    byte ledMode =
        2;  // Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
    int sampleRate = 400;  // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
    int pulseWidth = 411;  // Options: 69, 118, 215, 411
    int adcRange = 2048;   // Options: 2048, 4096, 8192, 16384

    prox_sensor_configure(ledBrightness, sampleAverage, ledMode, sampleRate,
                          pulseWidth,
                          adcRange);  // Configure sensor with these settings
    prox_sensor_pulse_amp_threshold(0, -1, 0);
    // Take an average of IR readings at power up
    unblockedValue = 0;
    for (byte x = 0; x < 32; x++) {
        uint32_t tmp;
        if (prox_sensor_read_raw(nullptr, &tmp, nullptr)) {
            unblockedValue += tmp;  // Read the IR value
        } else {
            --x;  // retry
        }
    }
    unblockedValue /= 32;

    startTime = millis();
}
void loop() {
    uint32_t ir;
    if (prox_sensor_read_raw(nullptr, &ir, nullptr)) {
        samplesTaken++;
        Serial.print("IR[");
        Serial.print(ir);
        Serial.print("] Hz[");
        Serial.print((float)samplesTaken / ((millis() - startTime) / 1000.0),
                     2);
        Serial.print("]");
        if (prox_sensor_read_raw(nullptr, &ir, nullptr)) {
            long currentDelta = ir - unblockedValue;

            Serial.print(" delta[");
            Serial.print(currentDelta);
            Serial.print("]");

            if (currentDelta > (long)200) {
                Serial.print(" Something is there!");
            }

            Serial.println();
        }
    }
}