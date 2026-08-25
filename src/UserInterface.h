#pragma once

#include <Arduino.h>
#include <RotaryEncoder.h>

#include "InputEvent.h"
#include "SystemState.h"


class UserInterface {
    public:
        UserInterface(const SystemState* systemState,
                      uint8_t            encoderPinA,
                      uint8_t            encoderPinB,
                      uint8_t            encoderButtonPin);

        void       begin(); // Does not mutate SystemState.
        InputEvent tick();

    private:
        enum class Screen : uint8_t {
            Dashboard,
            MainMenu,
            PidMenu,
            GainEditor,
            Placeholder
        };

        const SystemState* systemState_;

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

        int        readEncoderDelta();
        InputEvent readInputEvent(uint32_t nowMs);

        bool containsLiveData(Screen screen) const;
        void openScreen(Screen screen);

        static void moveSelection(
                int      encoderDelta,
                uint8_t  itemCount,
                uint8_t& selectedItem,
                uint8_t& topItem
                );

        // Each screen receives input every tick. The boolean only controls
        // whether OLED drawing is allowed during that tick.
        bool renderDashboard(
                const InputEvent& inputEvent,
                bool              displayUpdateAllowed
                );
        bool renderMainMenu(
                const InputEvent& inputEvent,
                bool              displayUpdateAllowed
                );
        bool renderPidMenu(
                const InputEvent& inputEvent,
                bool              displayUpdateAllowed
                );
        bool renderGainEditor(
                const InputEvent& inputEvent,
                bool              displayUpdateAllowed
                );
        bool renderPlaceholder(
                const InputEvent& inputEvent,
                bool              displayUpdateAllowed
                );

        void beginDisplayUpdate();

        static void renderMenu(
                const char*        title,
                const char* const* items,
                uint8_t            itemCount,
                uint8_t            selectedItem,
                uint8_t            topItem
                );
};
