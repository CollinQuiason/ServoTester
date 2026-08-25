#pragma once

#include <Arduino.h>
#include <RotaryEncoder.h>
#include "SystemState.h"
#include "InputEvent.h"


class UserInterface {
    public:
        UserInterface(const SystemState* systemState,
                      const uint8_t      encoderPinA,
                      const uint8_t      encoderPinB,
                      const uint8_t      encoderButtonPin);

        void begin(); // No mutation inside of UI
        InputEvent tick();

    private:
        const SystemState* systemState_;

        enum class Screen : uint8_t {
            Dashboard,
            MainMenu,
            PidMenu,
            GainEditor,
            Placeholder
        };

        enum class ButtonEvent : uint8_t {
            None,
            Click,
            LongPress
        };

        RotaryEncoder encoder_;
        const uint8_t buttonPin_;

        Screen screen_ = Screen::Dashboard;

        long previousEncoderPosition_ = 0;

        bool     previousRawButtonPressed_ = false;
        bool     stableButtonPressed_      = false;
        bool     longPressReported_        = false;
        uint32_t rawButtonChangedAtMs_     = 0;
        uint32_t buttonPressedAtMs_        = 0;

        uint8_t selectedMainItem_    = 0;
        uint8_t mainMenuTopItem_     = 0;
        uint8_t selectedPidItem_     = 0;
        uint8_t pidMenuTopItem_      = 0;
        uint8_t selectedGain_        = 0;
        uint8_t selectedPlaceholder_ = 0;

        bool     displayDirty_        = true;
        uint32_t lastDisplayUpdateMs_ = 0;

        int         readEncoderDelta();
        InputEvent readInputEvent(uint32_t nowMs);

        bool containsLiveData(Screen screen);
        void handleInput(int encoderDelta, ButtonEvent buttonEvent);
        void handleDashboard(ButtonEvent buttonEvent);
        void handleMainMenu(int encoderDelta, ButtonEvent buttonEvent);
        void handlePidMenu(int encoderDelta, ButtonEvent buttonEvent);
        void handleGainEditor(ButtonEvent buttonEvent);
        void handlePlaceholder(ButtonEvent buttonEvent);

        void openScreen(Screen screen);

        static void moveSelection(
                int      encoderDelta,
                uint8_t  itemCount,
                uint8_t& selectedItem,
                uint8_t& topItem
                );

        void updateDisplay(uint32_t nowMs);
        void renderDashboard();
        void renderMainMenu();
        void renderPidMenu();
        void renderGainEditor();
        void renderPlaceholder();

        static void renderMenu(
                const char*        title,
                const char* const* items,
                uint8_t            itemCount,
                uint8_t            selectedItem,
                uint8_t            topItem
                );
};
