#pragma once


#include <stdint.h>



class UserAction {
    public:
        enum class Type : uint8_t {
            None,
            AdjustVoltageSetPoint,
            AdjustVoltageSenseOffset1,
            AdjustVoltageSenseOffset2,
            AdjustServoAngle,
            AdjustPGain,
            AdjustIGain,
            AdjustDGain
        };

        UserAction() = default;

        static UserAction adjustVoltageSetPoint(float delta) {
            return UserAction(Type::AdjustVoltageSetPoint, delta);
        }

        static UserAction adjustVoltageSenseOffset1(float delta) {
            return UserAction(Type::AdjustVoltageSenseOffset1, delta);
        }

        static UserAction adjustVoltageSenseOffset2(float delta) {
            return UserAction(Type::AdjustVoltageSenseOffset2, delta);
        }

        static UserAction adjustServoAngle(float deltaDegrees) {
            return UserAction(Type::AdjustServoAngle, deltaDegrees);
        }

        static UserAction adjustPGain(float delta) {
            return UserAction(Type::AdjustPGain, delta);
        }

        static UserAction adjustIGain(float delta) {
            return UserAction(Type::AdjustIGain, delta);
        }

        static UserAction adjustDGain(float delta) {
            return UserAction(Type::AdjustDGain, delta);
        }

        Type type() const {
            return type_;
        }

        float delta() const {
            return delta_;
        }

    private:
        UserAction(Type type, float delta)
            : type_(type), delta_(delta) {
        }

        Type  type_  = Type::None;
        float delta_ = 0.0f;
};
