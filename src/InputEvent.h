#pragma once

struct InputEvent {
    int  encoderDelta             = 0;
    bool encoderButtonDown        = false;
    bool encoderButtonPressed     = false;
    bool encoderButtonReleased    = false;
    bool encoderButtonClicked     = false;
    bool encoderButtonLongPressed = false;

    bool noEvent() const {
        return encoderDelta == 0 &&
                !encoderButtonDown &&
                !encoderButtonPressed &&
                !encoderButtonReleased &&
                !encoderButtonClicked &&
                !encoderButtonLongPressed;
    }

    InputEvent& withEncoderDelta(int value) {
        encoderDelta = value;
        return *this;
    }

    InputEvent& withEncoderButtonDown(bool value = true) {
        encoderButtonDown = value;
        return *this;
    }

    InputEvent& withEncoderButtonPressed(bool value = true) {
        encoderButtonPressed = value;
        return *this;
    }

    InputEvent& withEncoderButtonReleased(bool value = true) {
        encoderButtonReleased = value;
        return *this;
    }

    InputEvent& withEncoderButtonClicked(bool value = true) {
        encoderButtonClicked = value;
        return *this;
    }

    InputEvent& withEncoderButtonLongPressed(bool value = true) {
        encoderButtonLongPressed = value;
        return *this;
    }
};
