#include <Arduino.h>
#include <Wire.h>

#include <RotaryEncoder.h>
#include "ssd1306.h"

/* ######## Config */
  // IO
constexpr uint32_t SCREEN_UPDATE_RATE_MS = 50;
constexpr uint8_t USB_OVER_UART_BAUD = 9600;
constexpr uint8_t PWM_RESOLUTION = 12;
constexpr uint32_t PWM_FREQUENCY = 1000;
  // Pin Definitions
constexpr uint8_t VOLTAGE_PIN_1 = 36;
constexpr uint8_t VOLTAGE_PIN_2 = 39;
constexpr uint8_t VOLTAGE_FEEDBACK_MODULATION_PIN_1 = 25;
constexpr uint8_t ENCODER_A = 27;
constexpr uint8_t ENCODER_B = 14;
constexpr uint8_t ENCODER_BUTTON = 13;
  // Voltage PID Control Net
constexpr float DIVIDER_RATIO = 16.0f;
constexpr float voltageNetPGain = 0.05f; // Duty cycle %s per volt
constexpr float voltageNetIGain = 1.0f;
constexpr float voltageNetDGain = 1.0f;
  // Encoder
constexpr uint32_t DEBOUNCE_MS = 25;
constexpr int8_t EDGES_PER_DETENT = 4;


/* ######## Globals */
  // Encoder
RotaryEncoder encoder(
    ENCODER_A,
    ENCODER_B,
    RotaryEncoder::LatchMode::FOUR3
);
long previousEncoderPosition = 0;
long selectedValue = 0;
  // Voltage modulation
float voltageSetPoint1 = 0.0f;
constexpr int PWM_MAX = (1 << PWM_RESOLUTION) - 1;
uint32_t voltageFeedbackDutyCycle = PWM_MAX;
bool buttonHeld() {
    return !digitalRead(ENCODER_BUTTON);
}


void setup() {
      // IO
    Serial.begin(USB_OVER_UART_BAUD);
    Serial.print("Initializing IO...\n");
    Wire.begin();
    Serial.print("IO initialized!\n");
      // Voltage Monitoring
    Serial.print("Initializing voltage sens/modulation net...\n");
    analogReadResolution(12);
    analogSetPinAttenuation(VOLTAGE_PIN_1, ADC_11db);
    analogSetPinAttenuation(VOLTAGE_PIN_2, ADC_11db);
    pinMode(VOLTAGE_FEEDBACK_MODULATION_PIN_1, OUTPUT);
    digitalWrite(VOLTAGE_FEEDBACK_MODULATION_PIN_1, HIGH); // Set high to drop voltage ASAP
    analogWriteResolution(PWM_RESOLUTION);
    analogWriteFrequency(PWM_FREQUENCY);
    Serial.print("Voltage sens/modulation net initialized!\n");
      // OLED Panel
    Serial.print("Initializing OLED panel...\n");
    ssd1306_128x64_i2c_init();
    ssd1306_flipHorizontal(1);
    ssd1306_flipVertical(1);
    ssd1306_fillScreen(0x00);
    ssd1306_setFixedFont(ssd1306xled_font6x8);
    Serial.print("OLED panel initialized!\n");
      // Encoder
    Serial.print("Initializing encoder...\n");
    pinMode(ENCODER_BUTTON, INPUT_PULLUP);
    Serial.print("Encoder initialized!\n");
      // Done
    Serial.print("Setup done!\n");
}

void loop() {
    /* ######## Tick Layer */
      // Encoder
    encoder.tick();
    long encoderPosition = encoder.getPosition();

    /* ######## Peripheral Measurement Layer */
      // LM2596 Buck voltage sens
    float rawMV1 = analogReadMilliVolts(VOLTAGE_PIN_1);
    float rawMV2 = analogReadMilliVolts(VOLTAGE_PIN_2);

    /* ######## Compute Layer */
    // Voltage Control Net PID
    float adjustedV1 = rawMV1 * DIVIDER_RATIO / 1000.0f;
    float adjustedV2 = rawMV2 * DIVIDER_RATIO / 1000.0f;
    voltageSetPoint1 = (float)encoderPosition * 0.1f; // 0.1V per encoder position

    /* ######## Control Layer */
      // Voltage Control Net
        // Proportional control
    float voltageError1 = adjustedV1 - voltageSetPoint1;
    voltageFeedbackDutyCycle += voltageError1 * voltageNetPGain * (PWM_MAX / 100);
    voltageFeedbackDutyCycle = constrain(voltageFeedbackDutyCycle, 0 , PWM_MAX);
    analogWrite(VOLTAGE_FEEDBACK_MODULATION_PIN_1, voltageFeedbackDutyCycle);



    /* ######## UI Update Layer */
    static uint32_t lastScreenUpdate = 0;
    if (millis() - lastScreenUpdate >= SCREEN_UPDATE_RATE_MS) {

          // Formatting
            // LM2596 Buck voltage sens
        char rawVsens1Charp[10];
        char rawVsens2Charp[10];
        snprintf(rawVsens1Charp, sizeof(rawVsens1Charp), "%.3f", adjustedV1);
        snprintf(rawVsens2Charp, sizeof(rawVsens2Charp), "%.3f", adjustedV2);
            // Encoder
        char voltageSetpointCharp1[10];
        char voltageFeedbackDutyCycleCharp[10];
        snprintf(voltageSetpointCharp1, sizeof(voltageSetpointCharp1), "%.2f", (float) encoderPosition * 0.1f);
        snprintf(voltageFeedbackDutyCycleCharp, sizeof(voltageFeedbackDutyCycleCharp), "%d", voltageFeedbackDutyCycle);
          // Output
            // LM2596 Buck voltage sens
        ssd1306_printFixed(0, 8,  rawVsens1Charp, STYLE_NORMAL);
        ssd1306_printFixed(0, 16,  rawVsens2Charp, STYLE_NORMAL);
            // Encoder
        ssd1306_printFixed(0, 24, voltageSetpointCharp1, STYLE_NORMAL);
        ssd1306_printFixed(0, 32, voltageFeedbackDutyCycleCharp, STYLE_NORMAL);

        ssd1306_printFixed(0, 40, buttonHeld() ? "Pressed!" : "        ", STYLE_NORMAL);


        lastScreenUpdate = millis();
    }

    /* ######## Tick Layer */
}


