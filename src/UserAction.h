#pragma once

#include <stdint.h>


class UserAction {
    public:
        enum class Type : uint8_t {
            None,
            AdjustVoltageSetPoint,
            AdjustServoAngle,
            AdjustPGain,
            AdjustIGain,
            AdjustDGain
        };

        UserAction() = default;

        static UserAction adjustVoltageSetPoint(float delta) {
            return UserAction(Type::AdjustVoltageSetPoint, delta);
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