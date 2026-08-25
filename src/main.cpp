#include <Arduino.h>
#include <Wire.h>

#include <RotaryEncoder.h>
#include "ssd1306.h"

#include "SystemState.h"
#include "UserInterface.h"
#include "InputEvent.h"
#include "UserAction.h"

/* ######## Config */
// IO
constexpr uint32_t SCREEN_UPDATE_RATE_MS = 50;
constexpr uint32_t USB_OVER_UART_BAUD    = 9600;
constexpr uint8_t  PWM_RESOLUTION        = 12;
constexpr uint32_t PWM_FREQUENCY         = 5000;
// Pin Definitions
constexpr uint8_t VOLTAGE_PIN_1              = 36;
constexpr uint8_t VOLTAGE_PIN_2              = 39;
constexpr uint8_t VOLTAGE_FEEDBACK_NET_PIN_1 = 25;
constexpr uint8_t ENCODER_A                  = 27;
constexpr uint8_t ENCODER_B                  = 14;
constexpr uint8_t ENCODER_BUTTON             = 13;
// Voltage PID Control Net
constexpr float DIVIDER_RATIO = 16.0f;
// Encoder
constexpr uint32_t DEBOUNCE_MS      = 25;
constexpr int8_t   EDGES_PER_DETENT = 4;


/* ######## Globals */
SystemState systemState = {
            .v1 = 0.0f,
            .v2 = 0.0f,
            .vSetPoint1 = 0.0f,
            .vFBDutyCycle = 0.0f,
            .vError1 = 0.0f,
            .voltNetPGain = 1.7f, // Duty cycle % per volt
            .voltNetIGain = 0.7f,  // Duty cycle % per volt-second
            .voltNetDGain = 1.0f,
            .servoAngle = 90.0f
        };
UserInterface ui(
                 &systemState,
                 ENCODER_A,
                 ENCODER_B,
                 ENCODER_BUTTON);
// Voltage modulation
constexpr int PWM_MAX                    = (1 << PWM_RESOLUTION) - 1;
float         voltageIntegralDutyPercent = 100.0f;
uint32_t      previousVoltageControlUs   = 0;


void IRAM_ATTR handleEncoderInterrupt() {
    ui.onEncoderInterrupt();
}


void handleUserAction(const UserAction& action) {
    if (action.type() == UserAction::Type::None) {
        return;
    }
    switch (action.type()) {
        case UserAction::Type::AdjustVoltageSetPoint:
            systemState.vSetPoint1 += action.delta();
            break;

        case UserAction::Type::AdjustServoAngle:
            systemState.servoAngle = constrain(
                                               systemState.servoAngle + action.delta(),
                                               0.0f,
                                               180.0f
                                              );
            break;

        case UserAction::Type::AdjustPGain:
            systemState.voltNetPGain = constrain(
                                                 systemState.voltNetPGain + action.delta(),
                                                 0.0f,
                                                 999.999f
                                                );
            break;

        case UserAction::Type::AdjustIGain:
            systemState.voltNetIGain = constrain(
                                                 systemState.voltNetIGain + action.delta(),
                                                 0.0f,
                                                 999.999f
                                                );
            break;

        case UserAction::Type::AdjustDGain:
            systemState.voltNetDGain = constrain(
                                                 systemState.voltNetDGain + action.delta(),
                                                 0.0f,
                                                 999.999f
                                                );
            break;

        case UserAction::Type::None:
            break;
    }
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
    pinMode(VOLTAGE_FEEDBACK_NET_PIN_1, OUTPUT);
    digitalWrite(VOLTAGE_FEEDBACK_NET_PIN_1, HIGH); // Set high to drop voltage ASAP
    analogWriteResolution(PWM_RESOLUTION);
    analogWriteFrequency(PWM_FREQUENCY);
    systemState.vFBDutyCycle = PWM_MAX;
    Serial.print("Voltage sens/modulation net initialized!\n");
    // OLED Panel
    Serial.print("Initializing OLED panel...\n");
    ui.begin();
    Serial.print("OLED panel initialized!\n");
    // Encoder
    Serial.print("Initializing encoder...\n");
    pinMode(ENCODER_BUTTON, INPUT_PULLUP);
    attachInterrupt(
                    digitalPinToInterrupt(ENCODER_A),
                    handleEncoderInterrupt,
                    CHANGE
                   );
    attachInterrupt(
                    digitalPinToInterrupt(ENCODER_B),
                    handleEncoderInterrupt,
                    CHANGE
                   );
    Serial.print("Encoder initialized!\n");
    previousVoltageControlUs = micros();
    // Done
    Serial.print("Setup done!\n");
}

void loop() {
    /* ######## Tick Layer */

    /* ######## Peripheral Measurement Layer */
    // LM2596 Buck voltage sens
    float rawMV1 = analogReadMilliVolts(VOLTAGE_PIN_1);
    float rawMV2 = analogReadMilliVolts(VOLTAGE_PIN_2);

    /* ######## Compute Layer */
    // Voltage Control Net PID
    systemState.v1 = rawMV1 * DIVIDER_RATIO / 1000.0f;
    systemState.v2 = rawMV2 * DIVIDER_RATIO / 1000.0f;

    /* ######## Control Layer */
    // Voltage Control Net
    // PI control
    const uint32_t nowUs          = micros();
    const float    elapsedSeconds =
            (nowUs - previousVoltageControlUs) / 1000000.0f;
    previousVoltageControlUs = nowUs;

    systemState.vError1 = systemState.v1 - systemState.vSetPoint1;

    const float proportionalDutyPercent =
            systemState.vError1 * systemState.voltNetPGain;

    voltageIntegralDutyPercent +=
            systemState.vError1 *
            systemState.voltNetIGain *
            elapsedSeconds;
    voltageIntegralDutyPercent = constrain(
                                           voltageIntegralDutyPercent,
                                           0.0f,
                                           100.0f
                                          );

    const float dutyPercentDelta =
            proportionalDutyPercent + voltageIntegralDutyPercent;
    systemState.vFBDutyCycle = constrain(
                                         dutyPercentDelta * (PWM_MAX / 100.0f),
                                         0.0f,
                                         static_cast<float>(PWM_MAX)
                                        );
    analogWrite(VOLTAGE_FEEDBACK_NET_PIN_1, systemState.vFBDutyCycle);


    /* ######## UI Update Layer */
    handleUserAction(ui.tick());

    /* ######## Tick Layer */
}
