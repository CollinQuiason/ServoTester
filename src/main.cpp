#include <Arduino.h>
#include <Wire.h>
#include "ssd1306.h"

/* ######## Config */
  // IO
constexpr uint8_t USB_OVER_UART_BAUD = 115200;
  // Pin Definitions
constexpr uint8_t VOLTAGE_PIN_1 = 36;
constexpr uint8_t VOLTAGE_PIN_2 = 39;
  // Voltage monitoring on LM2596 bucks
constexpr float DIVIDER_RATIO = 16.0f;
/* ######## */


void drawLine() {
    char line [22];
    snprintf(line, sizeof(line), "Hello World!", "");
    ssd1306_printFixed(0, 0, line, STYLE_NORMAL);
}


void setup() {
      // IO
    Serial.begin(USB_OVER_UART_BAUD);
    Wire.begin();
      // Voltage Monitoring
    analogReadResolution(12);
    analogSetPinAttenuation(VOLTAGE_PIN_1, ADC_11db);
    analogSetPinAttenuation(VOLTAGE_PIN_2, ADC_11db);

    printf("Initializing OLED panel...");
    ssd1306_128x64_i2c_init();
    ssd1306_flipHorizontal(1);
    ssd1306_flipVertical(1);
    ssd1306_fillScreen(0x00);
    ssd1306_setFixedFont(ssd1306xled_font6x8);
    printf("OLED panel initialized!");
    // ssd1306_printFixed(0, 8, "Hello World!", STYLE_NORMAL);
}

int count = 0;
void loop() {
    char rawVsens1Charp[10];
    char rawVsens2Charp[10];
    float rawMV1 = analogReadMilliVolts(VOLTAGE_PIN_1);
    float rawMV2 = analogReadMilliVolts(VOLTAGE_PIN_2);
    float adjustedMV1 = rawMV1 * DIVIDER_RATIO;
    float adjustedMV2 = rawMV2 * DIVIDER_RATIO;
    snprintf(rawVsens1Charp, sizeof(rawVsens1Charp), "%f", adjustedMV1);
    snprintf(rawVsens2Charp, sizeof(rawVsens2Charp), "%f", adjustedMV2);
    ssd1306_printFixed(0, 8,  rawVsens1Charp, STYLE_NORMAL);
    ssd1306_printFixed(0, 16,  rawVsens2Charp, STYLE_NORMAL);
    // ssd1306_printFixed(0, 8, (char*) analogReadMilliVolts(VOLTAGE_PIN_1), STYLE_NORMAL);
    // ssd1306_printFixed(0, 16, (char*) analogReadMilliVolts(VOLTAGE_PIN_2), STYLE_NORMAL);
    // drawLine();
}

