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
        const uint8_t      encoderPinA,
        const uint8_t      encoderPinB,
        const uint8_t      encoderButtonPin
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
    ssd1306_clearScreen();

    displayDirty_ = true;
    updateDisplay(millis());
}

bool UserInterface::containsLiveData(Screen screen) {
    return screen_ == Screen::Dashboard ||
            screen_ == Screen::PidMenu ||
            screen_ == Screen::GainEditor;
}


InputEvent UserInterface::tick() {
    const uint32_t   nowMs        = millis();
    const int        encoderDelta = readEncoderDelta();
    const inputEvent inputEvent   = readInputEvent(nowMs);

    if (encoderDelta != 0 || inputEvent != ButtonEvent::None) {
        handleInput(encoderDelta, inputEvent);
    }

    updateDisplay(nowMs);
    return
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
            buttonPressedAtMs_ = nowMs;
            longPressReported_ = false;
        }
        else if (!longPressReported_) {
            InputEvent event;
            event.encoderButtonClicked = true;
            return event;
        }
    }

    if (
        stableButtonPressed_ &&
        !longPressReported_ &&
        nowMs - buttonPressedAtMs_ >= BUTTON_LONG_PRESS_MS
    ) {
        longPressReported_ = true;
        return InputEvent().withEncoderButtonLongPressed();
    }

    return InputEvent();
}

void UserInterface::handleInput(
        int         encoderDelta,
        ButtonEvent buttonEvent
        ) {
    switch (screen_) {
        case Screen::Dashboard:
            handleDashboard(buttonEvent);
            break;

        case Screen::MainMenu:
            handleMainMenu(encoderDelta, buttonEvent);
            break;

        case Screen::PidMenu:
            handlePidMenu(encoderDelta, buttonEvent);
            break;

        case Screen::GainEditor:
            handleGainEditor(buttonEvent);
            break;

        case Screen::Placeholder:
            handlePlaceholder(buttonEvent);
            break;
    }
}

void UserInterface::handleDashboard(ButtonEvent buttonEvent) {
    if (buttonEvent == ButtonEvent::LongPress) {
        openScreen(Screen::MainMenu);
    }
}

void UserInterface::handleMainMenu(
        int         encoderDelta,
        ButtonEvent buttonEvent
        ) {
    if (encoderDelta != 0) {
        moveSelection(
                      encoderDelta,
                      MAIN_MENU_ITEM_COUNT,
                      selectedMainItem_,
                      mainMenuTopItem_
                     );
        displayDirty_ = true;
    }

    if (buttonEvent == ButtonEvent::LongPress) {
        openScreen(Screen::Dashboard);
        return;
    }

    if (buttonEvent != ButtonEvent::Click) {
        return;
    }

    if (selectedMainItem_ == 0) {
        openScreen(Screen::PidMenu);
        return;
    }

    selectedPlaceholder_ = selectedMainItem_;
    openScreen(Screen::Placeholder);
}

void UserInterface::handlePidMenu(
        int         encoderDelta,
        ButtonEvent buttonEvent
        ) {
    if (encoderDelta != 0) {
        moveSelection(
                      encoderDelta,
                      PID_MENU_ITEM_COUNT,
                      selectedPidItem_,
                      pidMenuTopItem_
                     );
        displayDirty_ = true;
    }

    if (buttonEvent == ButtonEvent::LongPress) {
        openScreen(Screen::MainMenu);
        return;
    }

    if (buttonEvent == ButtonEvent::Click) {
        selectedGain_ = selectedPidItem_;
        openScreen(Screen::GainEditor);
    }
}

void UserInterface::handleGainEditor(ButtonEvent buttonEvent) {
    // The live digit editor will be added here next.
    if (buttonEvent == ButtonEvent::LongPress) {
        openScreen(Screen::PidMenu);
    }
}

void UserInterface::handlePlaceholder(ButtonEvent buttonEvent) {
    if (buttonEvent == ButtonEvent::LongPress) {
        openScreen(Screen::MainMenu);
    }
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

void UserInterface::updateDisplay(uint32_t nowMs) {
    if (!displayDirty_ && !containsLiveData(screen_)) {
        return;
    }
    if (displayDirty_) {
        ssd1306_clearScreen();
    }
    if (nowMs - lastDisplayUpdateMs_ < DISPLAY_UPDATE_INTERVAL_MS) {
        return;
    }

    lastDisplayUpdateMs_ = nowMs;
    displayDirty_        = false;
    switch (screen_) {
        case Screen::Dashboard:
            renderDashboard();
            break;

        case Screen::MainMenu:
            renderMainMenu();
            break;

        case Screen::PidMenu:
            renderPidMenu();
            break;

        case Screen::GainEditor:
            renderGainEditor();
            break;

        case Screen::Placeholder:
            renderPlaceholder();
            break;
    }
}

void UserInterface::renderDashboard() {
    // Formatting
    // LM2596 Buck voltage sens
    char v1CharP[10];
    char v2CharP[10];
    snprintf(v1CharP, sizeof(v1CharP), "%.3f", this->systemState_->v1);
    snprintf(v2CharP, sizeof(v2CharP), "%.3f", systemState_->v2); // See if the difference makes a difference
    // Encoder
    char vSetPointCharP[10];
    char vFBDutyCycleCharP[10];
    snprintf(vSetPointCharP, sizeof(vSetPointCharP), "%.2f", (float)encoder_.getPosition() * 0.1f);
    snprintf(vFBDutyCycleCharP, sizeof(vFBDutyCycleCharP), "%f", this->systemState_->vFBDutyCycle);
    // Output
    // LM2596 Buck voltage sens
    ssd1306_printFixed(0, 8, v1CharP, STYLE_NORMAL);
    ssd1306_printFixed(0, 16, v2CharP, STYLE_NORMAL);
    // Encoder
    ssd1306_printFixed(0, 24, vSetPointCharP, STYLE_NORMAL);
    ssd1306_printFixed(0, 32, vFBDutyCycleCharP, STYLE_NORMAL);

    ssd1306_printFixed(0, 40, readInputEvent(millis()) == ButtonEvent::LongPress ? "Pressed!" : "        ",
                       STYLE_NORMAL);
}

void UserInterface::renderMainMenu() {
    renderMenu(
               "MAIN MENU",
               MAIN_MENU_ITEMS,
               MAIN_MENU_ITEM_COUNT,
               selectedMainItem_,
               mainMenuTopItem_
              );
}

void UserInterface::renderPidMenu() {
    renderMenu(
               "PID TUNING",
               PID_MENU_ITEMS,
               PID_MENU_ITEM_COUNT,
               selectedPidItem_,
               pidMenuTopItem_
              );
}

void UserInterface::renderGainEditor() {
    ssd1306_printFixed(0, 0, PID_MENU_ITEMS[selectedGain_], STYLE_NORMAL);
    ssd1306_printFixed(0, 20, "Digit editor next", STYLE_NORMAL);
    ssd1306_printFixed(0, 48, "Hold: back", STYLE_NORMAL);
}

void UserInterface::renderPlaceholder() {
    ssd1306_printFixed(
                       0,
                       0,
                       MAIN_MENU_ITEMS[selectedPlaceholder_],
                       STYLE_NORMAL
                      );
    ssd1306_printFixed(0, 20, "Not implemented", STYLE_NORMAL);
    ssd1306_printFixed(0, 48, "Hold: back", STYLE_NORMAL);
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
                 "%c %s",
                 itemIndex == selectedItem ? '>' : ' ',
                 items[itemIndex]
                );

        ssd1306_printFixed(
                           0,
                           MENU_FIRST_ROW_Y + row * MENU_ROW_HEIGHT,
                           line,
                           STYLE_NORMAL
                          );
    }
}
