#include <Arduino.h>
#include <Wire.h>

constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;

void scanI2C()
{
    int devicesFound = 0;

    Serial.println("Scanning I2C bus...");

    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        Serial.println("beginning transmission");
        uint8_t result = Wire.endTransmission();

        if (result == 0) {
            Serial.printf(
                "Found device at 0x%02X\n",
                address
            );

            devicesFound++;
        }
    }

    if (devicesFound == 0) {
        Serial.println("No I2C devices found.");
    } else {
        Serial.printf(
            "Scan complete: %d device(s) found.\n",
            devicesFound
        );
    }
}

void setup()
{
    Serial.begin(9600);
    delay(1000);

    Wire.begin(SDA_PIN, SCL_PIN);
}

void loop()
{
    scanI2C();
    Serial.println();

    delay(3000);
}