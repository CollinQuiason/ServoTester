#include <Arduino.h>
#include <Wire.h>

#include "ESP32Encoder.h"
#include "ssd1306.h"

/* ######## Config */
  // IO
constexpr uint8_t USB_OVER_UART_BAUD = 9600;
  // Pin Definitions
constexpr uint8_t VOLTAGE_PIN_1 = 36;
constexpr uint8_t VOLTAGE_PIN_2 = 39;
constexpr uint8_t ENCODER_A = 14;
constexpr uint8_t ENCODER_B = 27;
constexpr uint8_t ENCODER_BUTTON = 13;
  // Voltage monitoring on LM2596 bucks
constexpr float DIVIDER_RATIO = 16.0f;
  // Encoder
constexpr uint32_t DEBOUNCE_MS = 25;
constexpr int8_t EDGES_PER_DETENT = 4;


/* ######## Globals */
  // Encoder
ESP32Encoder encoder;

bool buttonHeld() {
    return !digitalRead(ENCODER_BUTTON);
}


void setup() {
      // IO
    printf("Initializing IO...\n");
    Serial.begin(USB_OVER_UART_BAUD);
    Wire.begin();
    printf("IO initialized!\n");
      // Voltage Monitoring
    printf("Initializing voltage sens net...\n");
    analogReadResolution(12);
    analogSetPinAttenuation(VOLTAGE_PIN_1, ADC_11db);
    analogSetPinAttenuation(VOLTAGE_PIN_2, ADC_11db);
    printf("Voltage sens net initialized!\n");
      // OLED Panel
    printf("Initializing OLED panel...\n");
    ssd1306_128x64_i2c_init();
    ssd1306_flipHorizontal(1);
    ssd1306_flipVertical(1);
    ssd1306_fillScreen(0x00);
    ssd1306_setFixedFont(ssd1306xled_font6x8);
    printf("OLED panel initialized!\n");
      // Encoder
    printf("Initializing encoder...\n");
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachFullQuad(ENCODER_A, ENCODER_B);
    encoder.setFilter(1023);
    encoder.clearCount();
    pinMode(ENCODER_BUTTON, INPUT_PULLUP);
    printf("Encoder initialized!\n");
      // Done
    printf("Setup done!\n");
}

// void loop() {
//     float rawMV1 = analogReadMilliVolts(VOLTAGE_PIN_1);
//     float rawMV2 = analogReadMilliVolts(VOLTAGE_PIN_2);
//     float adjustedV1 = rawMV1 * DIVIDER_RATIO / 1000.0f;
//     float adjustedV2 = rawMV2 * DIVIDER_RATIO / 1000.0f;
//
//     /* ######## Screen Updates */
//       // Formatting
//     char rawVsens1Charp[10];
//     char rawVsens2Charp[10];
//     char selectedValueCharp[10];
//     snprintf(rawVsens1Charp, sizeof(rawVsens1Charp), "%.3f", adjustedV1);
//     snprintf(rawVsens2Charp, sizeof(rawVsens2Charp), "%.3f", adjustedV2);
//       // Output
//     ssd1306_printFixed(0, 8,  rawVsens1Charp, STYLE_NORMAL);
//     ssd1306_printFixed(0, 16,  rawVsens2Charp, STYLE_NORMAL);
//
//
//
//     if (buttonHeld()) {
//         ssd1306_printFixed(0, 32, "Pressed!", STYLE_NORMAL);
//     } else {
//         ssd1306_printFixed(0, 32, "        ", STYLE_NORMAL);
//     }
//
//     printf(reinterpret_cast<const char*>(encoder.getCount()));
//
//     char interruptCountCharp[10];
//     snprintf(interruptCountCharp, sizeof(interruptCountCharp), "%d", encoder.getCount());
//     ssd1306_printFixed(0, 40,  interruptCountCharp, STYLE_NORMAL);
// }

void loop()
{
    encoder.tick();

    static long previous = 0;
    long position = encoder.getPosition();

    if (position != previous) {
        previous = position;
        // Update selected value.
    }
}