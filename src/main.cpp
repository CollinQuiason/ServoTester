#include <Arduino.h>
#include <Wire.h>
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
portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;

/* ######## Global Variables */
  // Encoder
volatile int32_t encoderEdgeDelta = 0;
volatile uint8_t previousAB = 0;


DRAM_ATTR static const int8_t QUADRATURE_TABLE[16] = { // Rejects invalid transitions caused by contact bounce.{
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0
};
/* ######## */
int interruptCount = 0;
void IRAM_ATTR encoderISR() {
    interruptCount++;
    uint8_t currentAB =
        (digitalRead(ENCODER_A) << 1) |
         digitalRead(ENCODER_B);

    uint8_t transition = (previousAB << 2) | currentAB;
    previousAB = currentAB;

    portENTER_CRITICAL_ISR(&encoderMux);
    encoderEdgeDelta += QUADRATURE_TABLE[transition];
    portEXIT_CRITICAL_ISR(&encoderMux);
}

int32_t readEncoderChange() {
    portENTER_CRITICAL(&encoderMux);
    int32_t newEdges = encoderEdgeDelta;
    encoderEdgeDelta = 0;
    portEXIT_CRITICAL(&encoderMux);

    static int32_t remainder = 0;
    int32_t detents = 0;

    remainder += newEdges;

    while (remainder >= EDGES_PER_DETENT) {
        remainder -= EDGES_PER_DETENT;
        detents++;
    }

    while (remainder <= -EDGES_PER_DETENT) {
        remainder += EDGES_PER_DETENT;
        detents--;
    }

    return detents;
}

bool buttonPressed() {

    static bool previousRaw = HIGH;
    static bool stableState = HIGH;
    static uint32_t lastChangeTime = 0;

    bool raw = digitalRead(ENCODER_BUTTON);

    if (raw != previousRaw) {
        previousRaw = raw;
        lastChangeTime = millis();
    }

    if (millis() - lastChangeTime >= DEBOUNCE_MS &&
        raw != stableState) {

        stableState = raw;

        if (stableState == LOW)
            return true;
        }

    return false;
}

bool buttonHeld() {
    return !digitalRead(ENCODER_BUTTON);
}

void drawLine() {
    char line [22];
    snprintf(line, sizeof(line), "Hello World!", "");
    ssd1306_printFixed(0, 0, line, STYLE_NORMAL);
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
    pinMode(ENCODER_A, INPUT_PULLUP);
    pinMode(ENCODER_B, INPUT_PULLUP);
    pinMode(ENCODER_BUTTON, INPUT_PULLUP);
    previousAB =
    (digitalRead(ENCODER_A) << 1) |
     digitalRead(ENCODER_B);
    attachInterrupt(
        digitalPinToInterrupt(ENCODER_A),
        encoderISR,
        CHANGE
    );
    attachInterrupt(
        digitalPinToInterrupt(ENCODER_B),
        encoderISR,
        CHANGE
    );
    printf("Encoder initialized!\n");
      // Done
    printf("Setup done!\n");
}

void loop() {
    float rawMV1 = analogReadMilliVolts(VOLTAGE_PIN_1);
    float rawMV2 = analogReadMilliVolts(VOLTAGE_PIN_2);
    float adjustedV1 = rawMV1 * DIVIDER_RATIO / 1000.0f;
    float adjustedV2 = rawMV2 * DIVIDER_RATIO / 1000.0f;

    static int32_t selectedValue = 0;

    int32_t movement = readEncoderChange();

    if (movement != 0) {
        selectedValue += movement;
        Serial.printf("Value: %ld\n", selectedValue);
    }

    if (buttonPressed()) {
        Serial.println("Encoder pressed");
    }



    /* ######## Screen Updates */
      // Formatting
    char rawVsens1Charp[10];
    char rawVsens2Charp[10];
    char selectedValueCharp[10];
    snprintf(rawVsens1Charp, sizeof(rawVsens1Charp), "%.3f", adjustedV1);
    snprintf(rawVsens2Charp, sizeof(rawVsens2Charp), "%.3f", adjustedV2);
    snprintf(selectedValueCharp, sizeof(selectedValueCharp), "%d", selectedValue);
      // Output
    ssd1306_printFixed(0, 8,  rawVsens1Charp, STYLE_NORMAL);
    ssd1306_printFixed(0, 16,  rawVsens2Charp, STYLE_NORMAL);
    ssd1306_printFixed(0, 24,  selectedValueCharp, STYLE_NORMAL);
    if (buttonHeld()) {
        ssd1306_printFixed(0, 32, "Pressed!", STYLE_NORMAL);
    } else {
        ssd1306_printFixed(0, 32, "        ", STYLE_NORMAL);
    }


    char interruptCountCharp[10];
    snprintf(interruptCountCharp, sizeof(interruptCountCharp), "%d", interruptCount);
    ssd1306_printFixed(0, 40,  interruptCountCharp, STYLE_NORMAL);
}

