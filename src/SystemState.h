#pragma once

struct SystemState {
    float v1;
    float v2;
    float voltageSenseOffset1;
    float voltageSenseOffset2;
    float vSetPoint1;
    float vFBDutyCycle;
    float vError1;
    float voltNetPGain;
    float voltNetIGain;
    float voltNetDGain;
    float servoAngle;
    float servoDegreesPerClick;
    float servoMaxAngle;
    int servoPulseUs;
};
