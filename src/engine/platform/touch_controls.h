#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <utility>

struct ImDrawList;

namespace pt {

// On-screen controls for a phone (iOS), after the Android launcher's touch overlay (ignusloki/pt-pc, GameActivity.java): a
// virtual SDL gamepad the game reads like any other pad, so the prompts, the menus and the original's controls need nothing
// of their own. The layout follows what is on screen: in play two sticks that appear where a thumb lands (MOVE on the left,
// LOOK on the right) with Menu, OK / Use and Zoom; in the menus the sticks go and the menu's own buttons come (directions,
// OK, Back, Resume, and per page the column switch or the camera inversion). Use shows only where the game takes it (the
// picture loop), Jack only while the game listens for the word. Whatever was used last rules: a gamepad's buttons or sticks
// hide the controls, a touch brings them back. Everything keeps clear of the notch, the Dynamic Island and the home
// indicator (the window's safe area).
class TouchControls {
public:
    enum class Mode { Gameplay, OriginalMenu, PcMenu };

    bool Init(SDL_Window* window);
    void Shutdown();
    // attached, and no real gamepad connected
    bool Active() const { return joystick_ != nullptr && !pad_active_; }
    // finger events; true when the controls took the event
    bool ProcessEvent(const SDL_Event& event);
    void SetMode(Mode mode);
    // Fast Walk is on (PC settings > Extras): play shows a Run button that switches the boost on and off
    void SetFastWalk(bool on) { fast_walk_ = on; }
    bool RunActive() const { return fast_walk_ && run_on_ && !pad_active_; }
    // the picture loop, where Use (the pad's bottom face button) is taken
    void SetUseAvailable(bool on) { use_available_ = on; }
    // the game listens for "Jack": play shows a Jack button that stands in for the word
    void SetJackAvailable(bool on) { jack_available_ = on; }
    bool TakeJack() { return std::exchange(jack_pressed_, false); }
    // the overlay, in ImGui's display coordinates
    void Draw(ImDrawList* draw, float width, float height) const;

private:
    enum Control { kMenu, kAccept, kBack, kZoom, kUp, kDown, kLeft, kRight, kColumn, kFlipY, kFlipX, kRun, kJack, kL1, kR1, kControlCount };
    struct Rect {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        bool Contains(float px, float py) const { return px >= x && px <= x + w && py >= y && py <= y + h; }
    };
    struct Stick {
        SDL_FingerID finger = 0;
        bool held = false;
        float cx = 0.0f, cy = 0.0f;  // where the thumb landed (normalized window coordinates)
        float x = 0.0f, y = 0.0f;    // deflection, -1 to 1
    };
    struct Finger {
        SDL_FingerID id = 0;
        int control = -1;
    };

    static void SDLCALL Update(void* userdata);
    void CheckGamepads();
    void UsePad(bool pad);
    void UpdateSafeArea() const;
    bool Visible(int control) const;
    Rect Layout(int control) const;
    // where a control sits: its side, its row, and its place in the row counted from the screen's edge
    struct Slot { bool left; int row; int order; };
    Slot SlotOf(int control) const;
    float Width(int control) const;
    int Arrow(int control) const;
    const char* Label(int control) const;
    int Button(int control) const;
    void Press(int control, bool down);
    void Release();

    SDL_Window* window_ = nullptr;
    // a gamepad was used last (the controls hide until a touch)
    bool pad_active_ = false;
    bool fast_walk_ = false;
    bool run_on_ = false;
    bool use_available_ = false;
    bool jack_available_ = false;
    bool jack_pressed_ = false;
    // the safe area's insets, as shares of the window (left, top, right, bottom)
    mutable float inset_[4] = {};
    SDL_JoystickID id_ = 0;
    SDL_Joystick* joystick_ = nullptr;
    Mode mode_ = Mode::Gameplay;
    mutable float aspect_ = 16.0f / 9.0f;  // the screen's, as Draw last saw it
    Stick move_;
    Stick look_;
    std::array<Finger, 10> fingers_{};
    std::array<uint32_t, kControlCount> held_{};
    // a tap shorter than the game's poll still reaches it: a button stays down at least this long
    std::array<uint64_t, kControlCount> until_{};
};

}
