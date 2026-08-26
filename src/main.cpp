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
constexpr float DIVIDER_RATIO                 = 16.0f;
constexpr float VOLTAGE_DUTY_BIAS_PERCENT     = 100.0f;
constexpr float VOLTAGE_D_FILTER_TIME_SECONDS = 0.01f;
// Encoder
constexpr uint32_t DEBOUNCE_MS      = 25;
constexpr int8_t   EDGES_PER_DETENT = 4;


/* ######## Globals */
SystemState systemState = {
            .v1 = 0.0f,
            .v2 = 0.0f,
            .voltageSenseOffset1 = -0.3f,
            .voltageSenseOffset2 = -0.3f,
            .vSetPoint1 = 0.0f,
            .vFBDutyCycle = 0.0f,
            .vError1 = 0.0f,
            .voltNetPGain = 0.05f, // Duty cycle % per volt
            .voltNetIGain = 1.0f,  // Duty cycle % per volt-second
            .voltNetDGain = 0.0f,  // Duty cycle %-seconds per volt
            .servoAngle = 90.0f
        };
UserInterface ui(
                 &systemState,
                 ENCODER_A,
                 ENCODER_B,
                 ENCODER_BUTTON);
// Voltage modulation
constexpr int PWM_MAX                  = (1 << PWM_RESOLUTION) - 1;
float         voltageIntegralOutput1   = 0.0f;
float         previousVoltage1         = 0.0f;
float         filteredVoltageSlope1    = 0.0f;
bool          voltageSlopeInitialized1 = false;
uint32_t      previousVoltageControlUs = 0;


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

        case UserAction::Type::AdjustVoltageSenseOffset1:
            systemState.voltageSenseOffset1 = constrain(
                    systemState.voltageSenseOffset1 + action.delta(),
                    -999.999f,
                    999.999f
                );
            break;

        case UserAction::Type::AdjustVoltageSenseOffset2:
            systemState.voltageSenseOffset2 = constrain(
                    systemState.voltageSenseOffset2 + action.delta(),
                    -999.999f,
                    999.999f
                );
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
    systemState.v1 = rawMV1 * DIVIDER_RATIO / 1000.0f +
            systemState.voltageSenseOffset1;
    systemState.v2 = rawMV2 * DIVIDER_RATIO / 1000.0f +
            systemState.voltageSenseOffset2;

    /* ######## Control Layer */
    // Voltage Control Net
    // Positional PID control
    const uint32_t nowUs          = micros();
    const float    elapsedSeconds =
            (nowUs - previousVoltageControlUs) / 1000000.0f;
    previousVoltageControlUs = nowUs;

    if (voltageSlopeInitialized1 && elapsedSeconds > 0.0f) {
        const float rawVoltageSlope1 =
                (systemState.v1 - previousVoltage1) / elapsedSeconds;
        const float filterAmount = elapsedSeconds /
                (VOLTAGE_D_FILTER_TIME_SECONDS + elapsedSeconds);

        filteredVoltageSlope1 += filterAmount *
                (rawVoltageSlope1 - filteredVoltageSlope1);
    }

    previousVoltage1         = systemState.v1;
    voltageSlopeInitialized1 = true;

    systemState.vError1                       = systemState.v1 - systemState.vSetPoint1;
    const float voltageOutputWithoutIntegral1 =
            VOLTAGE_DUTY_BIAS_PERCENT +
            systemState.vError1 * systemState.voltNetPGain +
            filteredVoltageSlope1 * systemState.voltNetDGain;

    if (systemState.voltNetIGain == 0.0f) {
        voltageIntegralOutput1 = 0.0f;
    }
    else if (elapsedSeconds > 0.0f) {
        const float integralChange =
                systemState.vError1 *
                systemState.voltNetIGain *
                elapsedSeconds;
        const float outputBeforeIntegralChange =
                voltageOutputWithoutIntegral1 + voltageIntegralOutput1;
        const bool outputWithinLimits =
                outputBeforeIntegralChange >= 0.0f &&
                outputBeforeIntegralChange <= 100.0f;
        const bool integralMovesTowardLimits =
                (outputBeforeIntegralChange > 100.0f &&
                    integralChange < 0.0f) ||
                (outputBeforeIntegralChange < 0.0f &&
                    integralChange > 0.0f);

        if (outputWithinLimits || integralMovesTowardLimits) {
            voltageIntegralOutput1 += integralChange;
            voltageIntegralOutput1 = constrain(
                                               voltageIntegralOutput1,
                                               -voltageOutputWithoutIntegral1,
                                               100.0f - voltageOutputWithoutIntegral1
                                              );
        }
    }

    const float voltageDutyPercent1 = constrain(
                                                voltageOutputWithoutIntegral1 + voltageIntegralOutput1,
                                                0.0f,
                                                100.0f
                                               );
    systemState.vFBDutyCycle =
            voltageDutyPercent1 * (PWM_MAX / 100.0f);
    analogWrite(VOLTAGE_FEEDBACK_NET_PIN_1, systemState.vFBDutyCycle);


    /* ######## UI Update Layer */
    handleUserAction(ui.tick());

    /* ######## Tick Layer */
}