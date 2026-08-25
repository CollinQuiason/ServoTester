#include "UserInterface.h"

#include "ssd1306.h"


namespace {
    constexpr uint32_t BUTTON_DEBOUNCE_MS         = 25;
    constexpr uint32_t BUTTON_LONG_PRESS_MS       = 700;
    constexpr uint32_t DISPLAY_UPDATE_INTERVAL_MS = 50;

    constexpr uint8_t MENU_FIRST_ROW_Y  = 14;
    constexpr uint8_t MENU_ROW_HEIGHT   = 10;
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
        lastDisplayUpdateMs_ = nowMs;
        displayDirty_        = false;
    }
}


InputEvent UserInterface::tick() {
    const uint32_t nowMs = millis();

    InputEvent inputEvent   = readInputEvent(nowMs);
    inputEvent.encoderDelta = readEncoderDelta();

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
        lastDisplayUpdateMs_ = nowMs;
        displayDirty_        = false;
    }

    return inputEvent;
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


bool UserInterface::containsLiveData(Screen screen) const {
    return screen == Screen::Dashboard ||
            screen == Screen::PidMenu ||
            screen == Screen::GainEditor;
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

    if (!displayUpdateAllowed) {
        return false;
    }

    beginDisplayUpdate();

    char line[22];

    snprintf(line, sizeof(line), "V1:   %7.3f V", systemState_->v1);
    ssd1306_printFixed(0, 8, line, STYLE_NORMAL);

    snprintf(line, sizeof(line), "V2:   %7.3f V", systemState_->v2);
    ssd1306_printFixed(0, 16, line, STYLE_NORMAL);

    snprintf(line, sizeof(line), "Set:  %7.2f V", systemState_->vSetPoint1);
    ssd1306_printFixed(0, 24, line, STYLE_NORMAL);

    snprintf(line, sizeof(line), "Duty: %9.1f", systemState_->vFBDutyCycle);
    ssd1306_printFixed(0, 32, line, STYLE_NORMAL);

    snprintf(line, sizeof(line), "Err:  %7.3f V", systemState_->vError1);
    ssd1306_printFixed(0, 40, line, STYLE_NORMAL);

    ssd1306_printFixed(0, 54, "Hold: menu", STYLE_NORMAL);

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
        selectedGain_ = selectedPidItem_;
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


bool UserInterface::renderGainEditor(
        const InputEvent& inputEvent,
        bool              displayUpdateAllowed
        ) {
    // The actual gain values do not exist in SystemState yet, so this screen
    // keeps the editor placeholder without inventing another source of truth.
    if (inputEvent.encoderButtonLongPressed) {
        openScreen(Screen::PidMenu);
        return false;
    }

    if (!displayUpdateAllowed) {
        return false;
    }

    beginDisplayUpdate();
    ssd1306_printFixed(0, 0, PID_MENU_ITEMS[selectedGain_], STYLE_NORMAL);
    ssd1306_printFixed(0, 20, "Digit editor next", STYLE_NORMAL);
    ssd1306_printFixed(0, 48, "Hold: back", STYLE_NORMAL);

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
    ssd1306_printFixed(0, 48, "Hold: back", STYLE_NORMAL);

    return true;
}


void UserInterface::beginDisplayUpdate() {
    if (displayDirty_) {
        ssd1306_clearScreen();
    }
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
