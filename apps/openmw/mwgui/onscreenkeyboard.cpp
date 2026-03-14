#include "onscreenkeyboard.hpp"

#include <MyGUI_EditBox.h>
#include <MyGUI_Gui.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_RenderManager.h>

#include <SDL.h>

#include <components/settings/values.hpp>
#include <components/widgets/numericeditbox.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"

namespace MWGui
{
    // Row 0-3 character layouts
    const char OnScreenKeyboard::sUnshifted[4][KEYS_PER_ROW] = {
        { '1', '2', '3', '4', '5', '6', '7', '8', '9', '0' },
        { 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p' },
        { 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '\'' },
        { 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-' },
    };

    const char OnScreenKeyboard::sShifted[4][KEYS_PER_ROW] = {
        { '!', '@', '#', '$', '%', '^', '&', '*', '(', ')' },
        { 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P' },
        { 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '"' },
        { 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_' },
    };

    OnScreenKeyboard::OnScreenKeyboard()
        : mMainWidget(nullptr)
        , mPreviewText(nullptr)
        , mTextCursor(0)
        , mCursorRow(1)
        , mCursorCol(0)
        , mShifted(false)
        , mDismissed(false)
        , mHeldDRow(0)
        , mHeldDCol(0)
        , mRepeatTimer(0)
        , mRepeatStarted(false)
    {
        for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < KEYS_PER_ROW; ++c)
                mKeys[r][c] = nullptr;

        for (int c = 0; c < KEYS_PER_ROW; ++c)
            mSpecialKeyType[c] = Key_None;

        buildKeyboard();
    }

    OnScreenKeyboard::~OnScreenKeyboard()
    {
        if (mMainWidget)
            MyGUI::Gui::getInstance().destroyWidget(mMainWidget);
    }

    void OnScreenKeyboard::buildKeyboard()
    {
        // Create the main container widget on the OnScreenKeyboard layer
        // Use BlackBG skin for a visible dark background
        mMainWidget = MyGUI::Gui::getInstance().createWidget<MyGUI::Widget>(
            "BlackBG", 0, 0, 1, 1, MyGUI::Align::Default, "OnScreenKeyboard");
        mMainWidget->setVisible(false);
        mMainWidget->setNeedKeyFocus(false);
        mMainWidget->setNeedMouseFocus(false);

        // Preview text bar at the top showing the buffered text
        mPreviewText = mMainWidget->createWidget<MyGUI::TextBox>(
            "SandText", 0, 0, 1, 1, MyGUI::Align::Default);
        mPreviewText->setNeedKeyFocus(false);
        mPreviewText->setNeedMouseFocus(false);
        mPreviewText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        mPreviewText->setFontName("Default");

        // Create button widgets for each key
        for (int r = 0; r < NUM_ROWS; ++r)
        {
            for (int c = 0; c < KEYS_PER_ROW; ++c)
            {
                mKeys[r][c] = mMainWidget->createWidget<MyGUI::Button>(
                    "MW_Button", 0, 0, 1, 1, MyGUI::Align::Default);
                mKeys[r][c]->setNeedKeyFocus(false);
                mKeys[r][c]->setNeedMouseFocus(false);
            }
        }

        // Define row 4 special key spans:
        // [0-1] = Shift, [2-6] = Space, [7-8] = Bksp, [9] = Done
        mSpecialKeyType[0] = Key_Shift;
        mSpecialKeyType[1] = Key_Shift;
        mSpecialKeyType[2] = Key_Space;
        mSpecialKeyType[3] = Key_Space;
        mSpecialKeyType[4] = Key_Space;
        mSpecialKeyType[5] = Key_Space;
        mSpecialKeyType[6] = Key_Space;
        mSpecialKeyType[7] = Key_Backspace;
        mSpecialKeyType[8] = Key_Backspace;
        mSpecialKeyType[9] = Key_Done;

        updateLabels();
    }

    void OnScreenKeyboard::updateLayout()
    {
        const MyGUI::IntSize& viewSize = MyGUI::RenderManager::getInstance().getViewSize();
        int viewW = viewSize.width;
        int viewH = viewSize.height;

        // Keyboard occupies bottom 40% of screen (includes preview bar)
        int kbHeight = static_cast<int>(viewH * 0.40f);
        int kbTop = viewH - kbHeight;

        mMainWidget->setPosition(0, kbTop);
        mMainWidget->setSize(viewW, kbHeight);

        int padding = 2;

        // Preview bar at top: one row's height
        int previewH = (kbHeight - padding * (NUM_ROWS + 2)) / (NUM_ROWS + 1);
        mPreviewText->setPosition(padding * 2, padding);
        mPreviewText->setSize(viewW - padding * 4, previewH);

        int gridTop = padding + previewH + padding;
        int gridHeight = kbHeight - gridTop;
        int keyW = (viewW - padding * (KEYS_PER_ROW + 1)) / KEYS_PER_ROW;
        int keyH = (gridHeight - padding * (NUM_ROWS + 1)) / NUM_ROWS;

        // Rows 0-3: uniform keys
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < KEYS_PER_ROW; ++c)
            {
                int x = padding + c * (keyW + padding);
                int y = gridTop + padding + r * (keyH + padding);
                mKeys[r][c]->setPosition(x, y);
                mKeys[r][c]->setSize(keyW, keyH);
                mKeys[r][c]->setVisible(true);
            }
        }

        // Row 4: special keys with spans
        int row4Y = gridTop + padding + 4 * (keyH + padding);

        auto setSpan = [&](int startCol, int endCol, int y) {
            int x = padding + startCol * (keyW + padding);
            int w = (endCol - startCol + 1) * keyW + (endCol - startCol) * padding;
            for (int c = startCol; c <= endCol; ++c)
            {
                if (c == startCol)
                {
                    mKeys[4][c]->setPosition(x, y);
                    mKeys[4][c]->setSize(w, keyH);
                    mKeys[4][c]->setVisible(true);
                }
                else
                {
                    mKeys[4][c]->setPosition(x, y);
                    mKeys[4][c]->setSize(0, 0);
                    mKeys[4][c]->setVisible(false);
                }
            }
        };

        setSpan(0, 1, row4Y); // Shift
        setSpan(2, 6, row4Y); // Space
        setSpan(7, 8, row4Y); // Bksp
        // Done: single key at col 9
        {
            int x = padding + 9 * (keyW + padding);
            mKeys[4][9]->setPosition(x, row4Y);
            mKeys[4][9]->setSize(keyW, keyH);
            mKeys[4][9]->setVisible(true);
        }
    }

    void OnScreenKeyboard::updateHighlight()
    {
        for (int r = 0; r < NUM_ROWS; ++r)
            for (int c = 0; c < KEYS_PER_ROW; ++c)
                if (mKeys[r][c])
                    mKeys[r][c]->setStateSelected(false);

        // Highlight the current key
        if (mCursorRow < 4)
        {
            mKeys[mCursorRow][mCursorCol]->setStateSelected(true);
        }
        else
        {
            // For row 4, highlight the primary button of the span
            int primaryCol = mCursorCol;
            SpecialKey type = mSpecialKeyType[mCursorCol];
            while (primaryCol > 0 && mSpecialKeyType[primaryCol - 1] == type)
                --primaryCol;
            mKeys[4][primaryCol]->setStateSelected(true);
        }
    }

    void OnScreenKeyboard::updateLabels()
    {
        const auto& chars = mShifted ? sShifted : sUnshifted;
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < KEYS_PER_ROW; ++c)
            {
                mKeys[r][c]->setCaption(std::string(1, chars[r][c]));
            }
        }

        // Row 4 special keys
        mKeys[4][0]->setCaption(mShifted ? "SHIFT" : "Shift");
        mKeys[4][2]->setCaption("Space");
        mKeys[4][7]->setCaption("Bksp");
        mKeys[4][9]->setCaption("Done");
    }

    void OnScreenKeyboard::updatePreview()
    {
        // Show buffer with a cursor indicator
        std::string display = mBuffer;
        if (mTextCursor <= display.size())
            display.insert(mTextCursor, "|");
        mPreviewText->setCaption(display);
    }

    void OnScreenKeyboard::show()
    {
        if (!Settings::gui().mOnScreenKeyboard)
            return;

        // Initialize buffer from the currently focused EditBox, if any
        mBuffer.clear();
        mTextCursor = 0;
        MyGUI::Widget* focused = MyGUI::InputManager::getInstance().getKeyFocusWidget();

        // Respect per-widget opt-out (e.g. alchemy auto-named potion, numeric sliders)
        if (focused
            && (focused->isUserString("DisableOSK") || focused->castType<Gui::NumericEditBox>(false)))
            return;
        if (focused)
        {
            MyGUI::EditBox* edit = focused->castType<MyGUI::EditBox>(false);
            if (edit)
            {
                mBuffer = edit->getCaption();
                mTextCursor = mBuffer.size();
            }
        }

        mDismissed = false;
        mMainWidget->setVisible(true);
        mCursorRow = 1;
        mCursorCol = 0;
        mShifted = false;
        mHeldDRow = 0;
        mHeldDCol = 0;
        mRepeatTimer = 0;
        mRepeatStarted = false;
        updateLayout();
        updateLabels();
        updateHighlight();
        updatePreview();
    }

    void OnScreenKeyboard::hide()
    {
        mMainWidget->setVisible(false);
        mHeldDRow = 0;
        mHeldDCol = 0;
    }

    bool OnScreenKeyboard::isVisible() const
    {
        return mMainWidget && mMainWidget->getVisible();
    }


    void OnScreenKeyboard::moveCursor(int dRow, int dCol)
    {
        int oldCol = mCursorCol;
        int oldRow = mCursorRow;

        mCursorRow += dRow;
        mCursorCol += dCol;

        // Wrap rows
        if (mCursorRow < 0)
            mCursorRow = NUM_ROWS - 1;
        else if (mCursorRow >= NUM_ROWS)
            mCursorRow = 0;

        // Wrap columns
        if (mCursorCol < 0)
            mCursorCol = KEYS_PER_ROW - 1;
        else if (mCursorCol >= KEYS_PER_ROW)
            mCursorCol = 0;

        // On row 4, skip over columns that belong to the same multi-column key span
        // so that e.g. pressing right once on Shift jumps past both Shift columns to Space.
        if (mCursorRow == 4 && oldRow == 4 && dCol != 0)
        {
            while (mSpecialKeyType[mCursorCol] == mSpecialKeyType[oldCol])
            {
                mCursorCol += dCol;
                if (mCursorCol < 0)
                    mCursorCol = KEYS_PER_ROW - 1;
                else if (mCursorCol >= KEYS_PER_ROW)
                    mCursorCol = 0;
            }
        }

        updateHighlight();
    }

    void OnScreenKeyboard::activateKey()
    {
        if (mCursorRow < 4)
        {
            const auto& chars = mShifted ? sShifted : sUnshifted;
            insertChar(chars[mCursorRow][mCursorCol]);
        }
        else
        {
            SpecialKey type = mSpecialKeyType[mCursorCol];
            switch (type)
            {
                case Key_Shift:
                    toggleShift();
                    break;
                case Key_Space:
                    insertChar(' ');
                    break;
                case Key_Backspace:
                    doBackspace();
                    break;
                case Key_Done:
                    commit();
                    break;
                default:
                    break;
            }
        }
    }

    void OnScreenKeyboard::insertChar(char c)
    {
        if (mTextCursor > mBuffer.size())
            mTextCursor = mBuffer.size();
        mBuffer.insert(mBuffer.begin() + mTextCursor, c);
        ++mTextCursor;
        updatePreview();
    }

    void OnScreenKeyboard::doBackspace()
    {
        if (mTextCursor > 0 && !mBuffer.empty())
        {
            --mTextCursor;
            mBuffer.erase(mTextCursor, 1);
            updatePreview();
        }
    }

    void OnScreenKeyboard::movTextCursor(bool left)
    {
        if (left)
        {
            if (mTextCursor > 0)
                --mTextCursor;
        }
        else
        {
            if (mTextCursor < mBuffer.size())
                ++mTextCursor;
        }
        updatePreview();
    }

    void OnScreenKeyboard::toggleShift()
    {
        mShifted = !mShifted;
        updateLabels();
    }

    void OnScreenKeyboard::commit()
    {
        // Write the buffered text into the focused EditBox
        MyGUI::Widget* focused = MyGUI::InputManager::getInstance().getKeyFocusWidget();
        if (focused)
        {
            MyGUI::EditBox* edit = focused->castType<MyGUI::EditBox>(false);
            if (edit)
                edit->setCaption(mBuffer);
        }
        mDismissed = true;
        hide();
        // Defocus the EditBox so controller buttons work for the parent dialog.
        // mDismissed prevents the auto-re-focus bounce from re-showing the OSK.
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(nullptr);
    }

    void OnScreenKeyboard::cancel()
    {
        // Discard buffer and just hide
        hide();
    }

    bool OnScreenKeyboard::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                mHeldDRow = -1;
                mHeldDCol = 0;
                mRepeatTimer = 0;
                mRepeatStarted = false;
                moveCursor(-1, 0);
                return true;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                mHeldDRow = 1;
                mHeldDCol = 0;
                mRepeatTimer = 0;
                mRepeatStarted = false;
                moveCursor(1, 0);
                return true;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                mHeldDRow = 0;
                mHeldDCol = -1;
                mRepeatTimer = 0;
                mRepeatStarted = false;
                moveCursor(0, -1);
                return true;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                mHeldDRow = 0;
                mHeldDCol = 1;
                mRepeatTimer = 0;
                mRepeatStarted = false;
                moveCursor(0, 1);
                return true;
            case SDL_CONTROLLER_BUTTON_A:
                activateKey();
                return true;
            case SDL_CONTROLLER_BUTTON_B:
                doBackspace();
                return true;
            case SDL_CONTROLLER_BUTTON_X:
                insertChar(' ');
                return true;
            case SDL_CONTROLLER_BUTTON_Y:
                toggleShift();
                return true;
            case SDL_CONTROLLER_BUTTON_START:
                commit();
                return true;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                movTextCursor(true);
                return true;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                movTextCursor(false);
                return true;
            default:
                return false;
        }
    }

    bool OnScreenKeyboard::onControllerButtonReleased(const SDL_ControllerButtonEvent& arg)
    {
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                mHeldDRow = 0;
                mRepeatTimer = 0;
                mRepeatStarted = false;
                return true;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                mHeldDCol = 0;
                mRepeatTimer = 0;
                mRepeatStarted = false;
                return true;
            case SDL_CONTROLLER_BUTTON_A:
            case SDL_CONTROLLER_BUTTON_B:
            case SDL_CONTROLLER_BUTTON_X:
            case SDL_CONTROLLER_BUTTON_Y:
            case SDL_CONTROLLER_BUTTON_START:
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                return true;
            default:
                return false;
        }
    }

    void OnScreenKeyboard::onFrame(float dt)
    {
        // Clear dismissed flag once the EditBox actually loses focus
        // (e.g., dialog closed). Don't clear while text input is still
        // active — that's the auto-re-focus bounce from our own defocus.
        if (mDismissed && !SDL_IsTextInputActive())
            mDismissed = false;

        if (!isVisible())
            return;

        // Auto-hide if user switches to keyboard/mouse
        if (!MWBase::Environment::get().getInputManager()->joystickLastUsed())
        {
            hide();
            return;
        }

        // D-pad key repeat
        if (mHeldDRow != 0 || mHeldDCol != 0)
        {
            mRepeatTimer += dt;
            if (!mRepeatStarted)
            {
                if (mRepeatTimer >= REPEAT_INITIAL_DELAY)
                {
                    mRepeatStarted = true;
                    mRepeatTimer -= REPEAT_INITIAL_DELAY;
                    moveCursor(mHeldDRow, mHeldDCol);
                }
            }
            else
            {
                while (mRepeatTimer >= REPEAT_INTERVAL)
                {
                    mRepeatTimer -= REPEAT_INTERVAL;
                    moveCursor(mHeldDRow, mHeldDCol);
                }
            }
        }
    }
}
