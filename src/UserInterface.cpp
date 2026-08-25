#include "UserInterface.h"

#include "ssd1306.h"


namespace {
    constexpr uint32_t BUTTON_DEBOUNCE_MS         = 25;
    constexpr uint32_t BUTTON_LONG_PRESS_MS       = 700;
    constexpr uint32_t DISPLAY_UPDATE_INTERVAL_MS = 50;
    constexpr float    VOLTAGE_SET_POINT_STEP     = 0.1f;
    constexpr float    SERVO_ANGLE_STEP           = 1.0f;

    constexpr float PID_GAIN_DIGIT_STEPS[] = {
                100.0f,
                 10.0f,
                  1.0f,
                  0.1f,
                  0.01f,
                  0.001f
            };

    constexpr uint8_t PID_GAIN_DIGIT_COUNT =
            sizeof(PID_GAIN_DIGIT_STEPS) / sizeof(PID_GAIN_DIGIT_STEPS[0]);

    constexpr uint8_t MENU_FIRST_ROW_Y  = 8;
    constexpr uint8_t MENU_ROW_HEIGHT   = 8;
    constexpr uint8_t MENU_VISIBLE_ROWS = 5;

    const char* const MAIN_MENU_ITEMS[] = {
                "PID Tuning",
                "Output Setup",
                "Signal Generator",
                "Display",
                "Diagnostics"
            };

    constexpr uint8_t MAIN_MENU_ITEM_COUNT =
            sizeof(MAIN_MENU_ITEMS) / sizeof(MAIN_MENU_ITEMS[0]);

    const char* const PID_MENU_ITEMS[] = {
                "Error Graph",
                "P Gain",
                "I Gain",
                "D Gain"
            };

    constexpr uint8_t PID_MENU_ITEM_COUNT =
            sizeof(PID_MENU_ITEMS) / sizeof(PID_MENU_ITEMS[0]);
}


UserInterface::UserInterface(
        const SystemState* systemState,
        uint8_t            encoderPinA,
        uint8_t            encoderPinB,
        uint8_t            encoderButtonPin
        )
    : systemState_(systemState),
      encoder_(
               encoderPinA,
               encoderPinB,
               RotaryEncoder::LatchMode::FOUR3
              ),
      buttonPin_(encoderButtonPin) {
}


void UserInterface::begin() {
    pinMode(buttonPin_, INPUT_PULLUP);

    encoder_.tick();
    previousEncoderPosition_ = encoder_.getPosition();

    const bool buttonPressed  = digitalRead(buttonPin_) == LOW;
    previousRawButtonPressed_ = buttonPressed;
    stableButtonPressed_      = buttonPressed;
    rawButtonChangedAtMs_     = millis();

    ssd1306_128x64_i2c_init();
    ssd1306_flipHorizontal(true);
    ssd1306_flipVertical(true);
    ssd1306_setFixedFont(ssd1306xled_font6x8);

    // The first frame is drawn immediately instead of waiting for the normal
    // display-update interval.
    const uint32_t nowMs = millis();
    if (renderDashboard(InputEvent(), true)) {
        renderStatusBar();
        lastDisplayUpdateMs_ = nowMs;
        displayDirty_        = false;
    }
}


void IRAM_ATTR UserInterface::onEncoderInterrupt() {
    encoder_.tick();
}


UserAction UserInterface::tick() {
    const uint32_t nowMs = millis();

    InputEvent inputEvent   = readInputEvent(nowMs);
    inputEvent.encoderDelta = readEncoderDelta();
    UserAction userAction   = createUserAction(inputEvent);

    if (screen_ == Screen::ErrorGraph) {
        if (errorGraphCount_ < 128) {
            const uint8_t index =
                    (errorGraphStart_ + errorGraphCount_) % 128;
            errorGraphHistory_[index] = systemState_->vError1;
            ++errorGraphCount_;
        }
        else {
            errorGraphHistory_[errorGraphStart_] = systemState_->vError1;
            errorGraphStart_ = (errorGraphStart_ + 1) % 128;
        }
    }

    const bool displayUpdateAllowed =
            (displayDirty_ || containsLiveData(screen_)) &&
            nowMs - lastDisplayUpdateMs_ >= DISPLAY_UPDATE_INTERVAL_MS;

    bool displayUpdated = false;

    switch (screen_) {
        case Screen::Dashboard:
            displayUpdated = renderDashboard(
                                             inputEvent,
                                             displayUpdateAllowed
                                            );
            break;

        case Screen::MainMenu:
            displayUpdated = renderMainMenu(
                                            inputEvent,
                                            displayUpdateAllowed
                                           );
            break;

        case Screen::PidMenu:
            displayUpdated = renderPidMenu(
                                           inputEvent,
                                           displayUpdateAllowed
                                          );
            break;

        case Screen::ErrorGraph:
            displayUpdated = renderErrorGraph(
                                                inputEvent,
                                                displayUpdateAllowed
                                               );
            break;

        case Screen::GainEditor:
            displayUpdated = renderGainEditor(
                                              inputEvent,
                                              displayUpdateAllowed
                                             );
            break;

        case Screen::Placeholder:
            displayUpdated = renderPlaceholder(
                                               inputEvent,
                                               displayUpdateAllowed
                                              );
            break;
    }

    if (displayUpdated) {
        renderStatusBar();
        lastDisplayUpdateMs_ = nowMs;
        displayDirty_        = false;
    }

    return userAction;
}


int UserInterface::readEncoderDelta() {
    encoder_.tick();

    const long position = encoder_.getPosition();
    const long delta    = position - previousEncoderPosition_;

    if (delta == 0) {
        return 0;
    }

    previousEncoderPosition_ = position;

    if (delta > 127) {
        return 127;
    }

    if (delta < -127) {
        return -127;
    }

    return static_cast<int>(delta);
}


InputEvent UserInterface::readInputEvent(uint32_t nowMs) {
    InputEvent inputEvent;

    const bool rawButtonPressed = digitalRead(buttonPin_) == LOW;

    if (rawButtonPressed != previousRawButtonPressed_) {
        previousRawButtonPressed_ = rawButtonPressed;
        rawButtonChangedAtMs_     = nowMs;
    }

    if (
        rawButtonPressed != stableButtonPressed_ &&
        nowMs - rawButtonChangedAtMs_ >= BUTTON_DEBOUNCE_MS
    ) {
        stableButtonPressed_ = rawButtonPressed;

        if (stableButtonPressed_) {
            buttonPressedAtMs_              = nowMs;
            longPressReported_              = false;
            inputEvent.encoderButtonPressed = true;
        }
        else {
            inputEvent.encoderButtonReleased = true;

            if (!longPressReported_) {
                inputEvent.encoderButtonClicked = true;
            }
        }
    }

    if (
        stableButtonPressed_ &&
        !longPressReported_ &&
        nowMs - buttonPressedAtMs_ >= BUTTON_LONG_PRESS_MS
    ) {
        longPressReported_                  = true;
        inputEvent.encoderButtonLongPressed = true;
    }

    inputEvent.encoderButtonDown = stableButtonPressed_;

    return inputEvent;
}


UserAction UserInterface::createUserAction(
        const InputEvent& inputEvent
        ) const {
    if (inputEvent.encoderDelta == 0) {
        return UserAction();
    }

    const float delta = static_cast<float>(inputEvent.encoderDelta);

    if (screen_ == Screen::ErrorGraph) {
        return UserAction::adjustVoltageSetPoint(
                                                 delta * VOLTAGE_SET_POINT_STEP
                                                );
    }

    if (screen_ == Screen::Dashboard) {
        if (selectedDashboardColumn_ == 0) {
            return UserAction::adjustVoltageSetPoint(
                                                     delta * VOLTAGE_SET_POINT_STEP
                                                    );
        }

        return UserAction::adjustServoAngle(delta * SERVO_ANGLE_STEP);
    }

    if (
        screen_ == Screen::GainEditor &&
        editingGainDigit_ &&
        !inputEvent.encoderButtonLongPressed
    ) {
        const float gainDelta =
                delta * PID_GAIN_DIGIT_STEPS[selectedGainDigit_];

        switch (selectedGain_) {
            case 0:
                return UserAction::adjustPGain(gainDelta);

            case 1:
                return UserAction::adjustIGain(gainDelta);

            case 2:
                return UserAction::adjustDGain(gainDelta);
        }
    }

    return UserAction();
}


bool UserInterface::containsLiveData(Screen) const {
    return true;
}


void UserInterface::openScreen(Screen screen) {
    screen_       = screen;
    displayDirty_ = true;
}


void UserInterface::moveSelection(
        int      encoderDelta,
        uint8_t  itemCount,
        uint8_t& selectedItem,
        uint8_t& topItem
        ) {
    if (itemCount == 0 || encoderDelta == 0) {
        return;
    }

    int nextItem = static_cast<int>(selectedItem) + encoderDelta;
    nextItem %= itemCount;

    if (nextItem < 0) {
        nextItem += itemCount;
    }

    selectedItem = static_cast<uint8_t>(nextItem);

    if (selectedItem < topItem) {
        topItem = selectedItem;
    }
    else if (selectedItem >= topItem + MENU_VISIBLE_ROWS) {
        topItem = selectedItem - MENU_VISIBLE_ROWS + 1;
    }
}


bool UserInterface::renderDashboard(
        const InputEvent& inputEvent,
        bool              displayUpdateAllowed
        ) {
    if (inputEvent.encoderButtonLongPressed) {
        openScreen(Screen::MainMenu);
        return false;
    }

    if (inputEvent.encoderButtonClicked) {
        selectedDashboardColumn_ = selectedDashboardColumn_ == 0 ? 1 : 0;
        displayDirty_            = true;
    }

    if (!displayUpdateAllowed) {
        return false;
    }

    beginDisplayUpdate();

    char line[11];

    if (selectedDashboardColumn_ == 0) {
        ssd1306_fillRect(0, 0, 60, 5);
    }
    else {
        ssd1306_fillRect(66, 0, 127, 5);
    }

    // Voltage column
    snprintf(line, sizeof(line), "VLT %5.2fV", systemState_->v1);
    ssd1306_printFixed(0, 8, line, STYLE_NORMAL);

    snprintf(line, sizeof(line), "LGV %5.2fV", systemState_->v2);
    ssd1306_printFixed(0, 16, line, STYLE_NORMAL);

    snprintf(line, sizeof(line), "DTY %6.0f", systemState_->vFBDutyCycle);
    ssd1306_printFixed(0, 24, line, STYLE_NORMAL);

    snprintf(line, sizeof(line), "GAT %6s", "ON");
    ssd1306_printFixed(0, 32, line, STYLE_NORMAL);

    // Output column
    snprintf(line, sizeof(line), "DEG %6.0f", systemState_->servoAngle);
    ssd1306_printFixed(66, 8, line, STYLE_NORMAL);

    snprintf(line, sizeof(line), "PWM %6s", "----us");
    ssd1306_printFixed(66, 16, line, STYLE_NORMAL);

    return true;
}


bool UserInterface::renderMainMenu(
        const InputEvent& inputEvent,
        bool              displayUpdateAllowed
        ) {
    if (inputEvent.encoderDelta != 0) {
        moveSelection(
                      inputEvent.encoderDelta,
                      MAIN_MENU_ITEM_COUNT,
                      selectedMainItem_,
                      mainMenuTopItem_
                     );
        displayDirty_ = true;
    }

    if (inputEvent.encoderButtonLongPressed) {
        openScreen(Screen::Dashboard);
        return false;
    }

    if (inputEvent.encoderButtonClicked) {
        if (selectedMainItem_ == 0) {
            openScreen(Screen::PidMenu);
        }
        else {
            selectedPlaceholder_ = selectedMainItem_;
            openScreen(Screen::Placeholder);
        }

        return false;
    }

    if (!displayUpdateAllowed) {
        return false;
    }

    beginDisplayUpdate();
    renderMenu(
               "MAIN MENU",
               MAIN_MENU_ITEMS,
               MAIN_MENU_ITEM_COUNT,
               selectedMainItem_,
               mainMenuTopItem_
              );

    return true;
}


bool UserInterface::renderPidMenu(
        const InputEvent& inputEvent,
        bool              displayUpdateAllowed
        ) {
    if (inputEvent.encoderDelta != 0) {
        moveSelection(
                      inputEvent.encoderDelta,
                      PID_MENU_ITEM_COUNT,
                      selectedPidItem_,
                      pidMenuTopItem_
                     );
        displayDirty_ = true;
    }

    if (inputEvent.encoderButtonLongPressed) {
        openScreen(Screen::MainMenu);
        return false;
    }

    if (inputEvent.encoderButtonClicked) {
        if (selectedPidItem_ == 0) {
            errorGraphStart_ = 0;
            errorGraphCount_ = 0;
            errorGraphScale_ = 0.1f;
            openScreen(Screen::ErrorGraph);
            return false;
        }

        selectedGain_      = selectedPidItem_ - 1;
        selectedGainDigit_ = 4;
        editingGainDigit_  = false;
        openScreen(Screen::GainEditor);
        return false;
    }

    if (!displayUpdateAllowed) {
        return false;
    }

    beginDisplayUpdate();
    renderMenu(
               "PID TUNING",
               PID_MENU_ITEMS,
               PID_MENU_ITEM_COUNT,
               selectedPidItem_,
               pidMenuTopItem_
              );

    return true;
}


bool UserInterface::renderErrorGraph(
        const InputEvent& inputEvent,
        bool              displayUpdateAllowed
        ) {
    if (inputEvent.encoderButtonLongPressed) {
        openScreen(Screen::PidMenu);
        return false;
    }

    if (!displayUpdateAllowed) {
        return false;
    }

    float maxAbsoluteError = 0.1f;

    for (uint16_t i = 0; i < errorGraphCount_; ++i) {
        const float error = errorGraphHistory_[
                (errorGraphStart_ + i) % 128
            ];
        const float absoluteError = error < 0.0f ? -error : error;

        if (absoluteError > maxAbsoluteError) {
            maxAbsoluteError = absoluteError;
        }
    }

    maxAbsoluteError *= 1.1f;

    if (maxAbsoluteError > errorGraphScale_) {
        errorGraphScale_ = maxAbsoluteError;
    }
    else {
        errorGraphScale_ *= 0.98f;

        if (errorGraphScale_ < maxAbsoluteError) {
            errorGraphScale_ = maxAbsoluteError;
        }
    }

    uint8_t graphBuffer[128 * 48 / 8] = {};

    // Dotted zero-error line.
    for (uint8_t x = 0; x < 128; x += 2) {
        graphBuffer[(24 / 8) * 128 + x] |= 1 << (24 % 8);
    }

    int previousY = 24;

    for (uint16_t i = 0; i < errorGraphCount_; ++i) {
        const uint8_t x = static_cast<uint8_t>(
                128 - errorGraphCount_ + i
            );
        const float error = errorGraphHistory_[
                (errorGraphStart_ + i) % 128
        ];
        int y = static_cast<int>(
                24.0f - error / errorGraphScale_ * 24.0f
            );
        y = constrain(y, 0, 47);

        const int firstY = i == 0 || y < previousY ? y : previousY;
        const int lastY  = i == 0 || y > previousY ? y : previousY;

        for (int lineY = firstY; lineY <= lastY; ++lineY) {
            graphBuffer[(lineY / 8) * 128 + x] |= 1 << (lineY % 8);
        }

        previousY = y;
    }

    ssd1306_drawBufferFast(0, 0, 128, 48, graphBuffer);

    return true;
}


bool UserInterface::renderGainEditor(
        const InputEvent& inputEvent,
        bool              displayUpdateAllowed
        ) {
    if (inputEvent.encoderButtonLongPressed) {
        editingGainDigit_ = false;
        openScreen(Screen::PidMenu);
        return false;
    }

    if (inputEvent.encoderButtonClicked) {
        editingGainDigit_ = !editingGainDigit_;
        displayDirty_     = true;
    }

    if (inputEvent.encoderDelta != 0 && !editingGainDigit_) {
        int nextDigit =
                static_cast<int>(selectedGainDigit_) + inputEvent.encoderDelta;
        nextDigit %= PID_GAIN_DIGIT_COUNT;

        if (nextDigit < 0) {
            nextDigit += PID_GAIN_DIGIT_COUNT;
        }

        selectedGainDigit_ = static_cast<uint8_t>(nextDigit);
        displayDirty_      = true;
    }

    if (!displayUpdateAllowed) {
        return false;
    }

    beginDisplayUpdate();

    float gain = 0.0f;

    switch (selectedGain_) {
        case 0:
            gain = systemState_->voltNetPGain;
            break;

        case 1:
            gain = systemState_->voltNetIGain;
            break;

        case 2:
            gain = systemState_->voltNetDGain;
            break;
    }

    char gainText[8];
    snprintf(gainText, sizeof(gainText), "%07.3f", gain);

    char line[22];
    snprintf(line, sizeof(line), "Value: %s", gainText);

    const uint8_t selectedDigitX = static_cast<uint8_t>(
            42 +
            (selectedGainDigit_ + (selectedGainDigit_ >= 3 ? 1 : 0)) * 6
        );

    ssd1306_printFixed(0, 0, PID_MENU_ITEMS[selectedGain_ + 1], STYLE_NORMAL);
    ssd1306_printFixed(66, 0, "Hold:back", STYLE_NORMAL);
    ssd1306_printFixed(0, 8, line, STYLE_NORMAL);
    ssd1306_printFixed(selectedDigitX, 16, "^", STYLE_NORMAL);
    ssd1306_printFixed(
                       0,
                       28,
                       editingGainDigit_ ?
                               "Turn: change digit" : "Turn: select digit",
                       STYLE_NORMAL
                      );
    ssd1306_printFixed(
                       0,
                       36,
                       editingGainDigit_ ?
                               "Click: select digit" : "Click: edit digit",
                       STYLE_NORMAL
                      );
    return true;
}


bool UserInterface::renderPlaceholder(
        const InputEvent& inputEvent,
        bool              displayUpdateAllowed
        ) {
    if (inputEvent.encoderButtonLongPressed) {
        openScreen(Screen::MainMenu);
        return false;
    }

    if (!displayUpdateAllowed) {
        return false;
    }

    beginDisplayUpdate();
    ssd1306_printFixed(
                       0,
                       0,
                       MAIN_MENU_ITEMS[selectedPlaceholder_],
                       STYLE_NORMAL
                      );
    ssd1306_printFixed(0, 20, "Not implemented", STYLE_NORMAL);
    ssd1306_printFixed(0, 40, "Hold: back", STYLE_NORMAL);

    return true;
}


void UserInterface::beginDisplayUpdate() {
    if (displayDirty_) {
        ssd1306_clearScreen();
    }
}


void UserInterface::renderStatusBar() const {
    char line[12];

    // Voltage column
    snprintf(
             line,
             sizeof(line),
             "SET %5.2fV",
             systemState_->vSetPoint1
            );
    ssd1306_printFixed(0, 48, line, STYLE_NORMAL);

    snprintf(line, sizeof(line), "ERR %6.3f", systemState_->vError1);
    ssd1306_printFixed(0, 56, line, STYLE_NORMAL);

    if (screen_ == Screen::ErrorGraph) {
        const float maximumVoltage =
                systemState_->vSetPoint1 + errorGraphScale_;
        const float minimumVoltage =
                systemState_->vSetPoint1 - errorGraphScale_;
        const int voltagePrecision = maximumVoltage < 100.0f ? 2 :
                maximumVoltage < 1000.0f ? 1 : 0;

        snprintf(
                 line,
                 sizeof(line),
                 "MAX %5.*fV",
                 voltagePrecision,
                 maximumVoltage
                );
        ssd1306_printFixed(66, 48, line, STYLE_NORMAL);

        snprintf(
                 line,
                 sizeof(line),
                 "MIN %5.*fV",
                 voltagePrecision,
                 minimumVoltage
                );
        ssd1306_printFixed(66, 56, line, STYLE_NORMAL);
        return;
    }

    // Output column
    snprintf(
             line,
             sizeof(line),
             "DEG %6.0f",
             systemState_->servoAngle
            );
    ssd1306_printFixed(66, 48, line, STYLE_NORMAL);

    snprintf(line, sizeof(line), "PWM %6s", "----us");
    ssd1306_printFixed(66, 56, line, STYLE_NORMAL);
}


void UserInterface::renderMenu(
        const char*        title,
        const char* const* items,
        uint8_t            itemCount,
        uint8_t            selectedItem,
        uint8_t            topItem
        ) {
    ssd1306_printFixed(0, 0, title, STYLE_NORMAL);

    for (uint8_t row = 0; row < MENU_VISIBLE_ROWS; ++row) {
        const uint8_t itemIndex = topItem + row;

        if (itemIndex >= itemCount) {
            break;
        }

        char line[22];
        snprintf(
                 line,
                 sizeof(line),
                 "%-1c %-18s",
                 itemIndex == selectedItem ? '>' : ' ',
                 items[itemIndex]
                );

        ssd1306_printFixed(
                           0,
                           static_cast<uint8_t>(
                               MENU_FIRST_ROW_Y + row * MENU_ROW_HEIGHT
                           ),
                           line,
                           STYLE_NORMAL
                          );
    }
}
