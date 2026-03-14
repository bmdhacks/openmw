#ifndef MWGUI_ONSCREENKEYBOARD_H
#define MWGUI_ONSCREENKEYBOARD_H

#include <string>

#include <SDL_events.h>

#include <MyGUI_Button.h>
#include <MyGUI_TextBox.h>
#include <MyGUI_Widget.h>

namespace MWGui
{
    class OnScreenKeyboard
    {
    public:
        OnScreenKeyboard();
        ~OnScreenKeyboard();

        void show();
        void hide();
        bool isVisible() const;
        bool isDismissed() const { return mDismissed; }


        /// Process a controller button press. Returns true if consumed.
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg);

        /// Process a controller button release (for d-pad repeat). Returns true if consumed.
        bool onControllerButtonReleased(const SDL_ControllerButtonEvent& arg);

        /// Called each frame for d-pad key repeat and auto-hide.
        void onFrame(float dt);

    private:
        void buildKeyboard();
        void updateLayout();
        void updateHighlight();
        void updateLabels();
        void updatePreview();

        void moveCursor(int dRow, int dCol);
        void activateKey();
        void insertChar(char c);
        void doBackspace();
        void movTextCursor(bool left);
        void toggleShift();
        void commit();
        void cancel();

        MyGUI::Widget* mMainWidget;
        MyGUI::TextBox* mPreviewText;

        // Buffered text and cursor position within it
        std::string mBuffer;
        size_t mTextCursor;

        // Grid of key buttons
        static constexpr int NUM_ROWS = 5;
        static constexpr int KEYS_PER_ROW = 10;
        MyGUI::Button* mKeys[NUM_ROWS][KEYS_PER_ROW];

        // Row 4 special keys: Shift(0-1), Space(2-6), Bksp(7-8), Done(9)
        enum SpecialKey
        {
            Key_None = 0,
            Key_Shift,
            Key_Space,
            Key_Backspace,
            Key_Done
        };
        SpecialKey mSpecialKeyType[KEYS_PER_ROW]; // for row 4 only

        int mCursorRow;
        int mCursorCol;
        bool mShifted;
        bool mDismissed;

        // D-pad repeat state
        int mHeldDRow;
        int mHeldDCol;
        float mRepeatTimer;
        bool mRepeatStarted;
        static constexpr float REPEAT_INITIAL_DELAY = 0.4f;
        static constexpr float REPEAT_INTERVAL = 0.08f;

        // Unshifted and shifted character layouts (rows 0-3)
        static const char sUnshifted[4][KEYS_PER_ROW];
        static const char sShifted[4][KEYS_PER_ROW];
    };
}

#endif
