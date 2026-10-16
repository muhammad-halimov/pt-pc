#include "engine/platform/touch_controls.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "engine/core/log.h"

namespace pt {
namespace {

// a stick's reach, as a share of the screen's height
constexpr float kStickRadius = 0.16f;
// where the sticks rest before a thumb lands (they then appear under it)
constexpr float kMoveHomeX = 0.07f, kLookHomeX = 0.93f, kStickHomeY = 0.78f;
// the band the sticks start in: the screen below the top row of buttons
constexpr float kStickTop = 0.27f;
// buttons as high as a line of text and as wide as their label, with a little room around it
constexpr float kButtonH = 0.075f, kRowStep = 0.09f, kGap = 0.01f, kPadX = 12.0f;
constexpr uint64_t kMinPressMs = 80;

}

bool TouchControls::Init(SDL_Window* window) {
    window_ = window;
    if (joystick_) {
        return true;
    }
    // a touch is the controls' alone: SDL's mouse made of the finger would also turn the camera and click the menus
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    desc.name = "P.T. touch controls";
    desc.Update = Update;
    desc.userdata = this;
    id_ = SDL_AttachVirtualJoystick(&desc);
    joystick_ = id_ ? SDL_OpenJoystick(id_) : nullptr;
    if (!joystick_) {
        LogError("touch: the virtual gamepad could not be attached: {}", SDL_GetError());
        if (id_) {
            SDL_DetachVirtualJoystick(id_);
        }
        id_ = 0;
        return false;
    }
    SDL_SetJoystickVirtualAxis(joystick_, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
    SDL_SetJoystickVirtualAxis(joystick_, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
    LogInfo("touch: on-screen controls attached as a virtual gamepad");
    CheckGamepads();
    return true;
}

void TouchControls::CheckGamepads() {
    int count = 0;
    SDL_JoystickID* pads = SDL_GetGamepads(&count);
    bool real = false;
    for (int i = 0; pads && i < count; ++i) {
        real = real || (pads[i] != id_ && !SDL_IsJoystickVirtual(pads[i]));
    }
    SDL_free(pads);
    // the last real gamepad gone: the controls come back
    if (!real && pad_active_) UsePad(false);
}

void TouchControls::UsePad(bool pad) {
    if (pad == pad_active_) return;
    pad_active_ = pad;
    Release();
    LogInfo("touch: {}", pad ? "a gamepad is in use, the on-screen controls hide" : "touch in use, the on-screen controls are back");
}

void TouchControls::UpdateSafeArea() const {
    int w = 0, h = 0;
    SDL_Rect safe{};
    if (!window_ || !SDL_GetWindowSize(window_, &w, &h) || w <= 0 || h <= 0 || !SDL_GetWindowSafeArea(window_, &safe)) {
        return;
    }
    inset_[0] = static_cast<float>(safe.x) / static_cast<float>(w);
    inset_[1] = static_cast<float>(safe.y) / static_cast<float>(h);
    inset_[2] = static_cast<float>(w - safe.x - safe.w) / static_cast<float>(w);
    inset_[3] = static_cast<float>(h - safe.y - safe.h) / static_cast<float>(h);
}

void TouchControls::Shutdown() {
    if (joystick_) {
        SDL_CloseJoystick(joystick_);
    }
    joystick_ = nullptr;
    if (id_) {
        SDL_DetachVirtualJoystick(id_);
    }
    id_ = 0;
}

void SDLCALL TouchControls::Update(void* userdata) {
    auto* self = static_cast<TouchControls*>(userdata);
    SDL_Joystick* joystick = self->joystick_;
    if (!joystick) {
        return;
    }
    const auto axis = [](float v) { return static_cast<Sint16>(std::clamp(v, -1.0f, 1.0f) * 32767.0f); };
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, axis(self->move_.x));
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, axis(self->move_.y));
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTX, axis(self->look_.x));
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTY, axis(self->look_.y));
    const uint64_t now = SDL_GetTicks();
    bool down[SDL_GAMEPAD_BUTTON_COUNT] = {};
    for (int c = 0; c < kControlCount; ++c) {
        const int button = self->Button(c);
        if (button >= 0 && (self->held_[c] > 0 || now < self->until_[c])) {
            down[button] = true;
        }
    }
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
        SDL_SetJoystickVirtualButton(joystick, b, down[b]);
    }
}

int TouchControls::Button(int control) const {
    switch (control) {
    case kMenu: return SDL_GAMEPAD_BUTTON_START;
    case kAccept: return SDL_GAMEPAD_BUTTON_SOUTH;
    case kBack: return SDL_GAMEPAD_BUTTON_EAST;
    case kZoom: return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    case kUp: return SDL_GAMEPAD_BUTTON_DPAD_UP;
    case kDown: return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    case kLeft: return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    case kRight: return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    case kColumn: return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    case kFlipY: return SDL_GAMEPAD_BUTTON_NORTH;
    case kFlipX: return SDL_GAMEPAD_BUTTON_WEST;
    case kL1: return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    case kR1: return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    default: return -1;  // Run and Jack are no pad buttons (RunActive, TakeJack)
    }
}

bool TouchControls::Visible(int control) const {
    const bool menu = mode_ != Mode::Gameplay;
    switch (control) {
    case kMenu: return true;
    case kAccept: return menu || use_available_;
    case kZoom: return mode_ != Mode::PcMenu;
    case kColumn: return false;  // L1 and R1 switch the column
    case kFlipY:
    case kFlipX: return mode_ == Mode::OriginalMenu;
    case kRun: return mode_ == Mode::Gameplay && fast_walk_;
    case kJack: return mode_ == Mode::Gameplay && jack_available_;
    case kL1:
    case kR1: return menu;
    default: return menu;
    }
}

TouchControls::Slot TouchControls::SlotOf(int control) const {
    if (mode_ == Mode::Gameplay) {
        // the top corners: Menu on the left; Zoom, Hurry, Jack and Use along the right
        switch (control) {
        case kMenu: return {true, 0, 0};
        case kZoom: return {false, 0, 0};
        case kRun: return {false, 0, 1};
        case kJack: return {false, 0, 2};
        case kAccept: return {false, 0, 3};
        default: return {false, 9, 0};
        }
    }
    // the menus: the directions down the left edge, the answers down the right
    switch (control) {
    case kMenu: return {true, 0, 0};
    case kUp: return {true, 1, 0};
    case kDown: return {true, 2, 0};
    case kLeft: return {true, 3, 0};
    case kRight: return {true, 4, 0};
    case kColumn: return {true, 5, 0};
    case kL1: return {true, 5, 0};
    case kR1: return {false, 5, 0};
    case kAccept: return {false, 0, 0};
    case kBack: return {false, 1, 0};
    case kZoom: return {false, 2, 0};
    case kFlipY: return {false, 3, 0};
    case kFlipX: return {false, 4, 0};
    default: return {false, 9, 0};
    }
}

// the menus' stacked buttons carry an arrow before their label (up, down, left, right, up and down, left and right)
int TouchControls::Arrow(int control) const {
    if (mode_ == Mode::Gameplay) return 0;
    switch (control) {
    case kUp: return 1;
    case kDown: return 2;
    case kLeft: return 3;
    case kRight: return 4;
    case kFlipY: return 5;
    case kFlipX: return 6;
    default: return 0;
    }
}

float TouchControls::Width(int control) const {
    if (!ImGui::GetCurrentContext() || ImGui::GetIO().DisplaySize.x <= 0.0f) return 0.08f;
    const ImGuiIO& io = ImGui::GetIO();
    const float arrow = kButtonH * io.DisplaySize.y * 0.7f;
    const auto pixels = [&](int c) { return ImGui::CalcTextSize(Label(c)).x + 2.0f * kPadX + (Arrow(c) ? arrow : 0.0f); };
    if (Arrow(control)) {
        // a stack's buttons share one width, its widest label's
        float widest = 0.0f;
        for (int c : {kUp, kDown, kLeft, kRight, kFlipY, kFlipX}) {
            if ((c == kFlipY || c == kFlipX) == (control == kFlipY || control == kFlipX)) widest = std::max(widest, pixels(c));
        }
        return widest / io.DisplaySize.x;
    }
    return pixels(control) / io.DisplaySize.x;
}

TouchControls::Rect TouchControls::Layout(int control) const {
    if (mode_ != Mode::Gameplay) {
        // the menus: the shoulders in the top corners, Resume and Zoom beside L1, Settings (OK) and Back beside R1; the
        // directions stacked down the left edge, the camera inversion down the right
        const float h = kButtonH, gap = kGap * 1.5f, w = Width(control);
        const float top = 0.03f + inset_[1];
        const float left = 0.02f + inset_[0], right = 0.98f - inset_[2];
        // the stacks stand on the bottom corners, their last button lowest
        const float bottom = 0.97f - inset_[3];
        const ImVec2 display = ImGui::GetCurrentContext() ? ImGui::GetIO().DisplaySize : ImVec2(0.0f, 0.0f);
        const float pair_gx = display.x > 0.0f ? 6.0f / display.x : gap;
        const float pair_gy = display.y > 0.0f ? 6.0f / display.y : gap;
        const float pair_bottom = std::min(0.84f, bottom);
        const float pc_row_bottom = std::min(0.905f, bottom);
        const auto stack = [&](bool on_left, int index) -> Rect {
            const int count = on_left ? 4 : 2;
            const float y = bottom - (h + gap) * static_cast<float>(count - index) + gap;
            return {on_left ? left : right - w, y, w, h};
        };
        switch (control) {
        case kL1: return {left, top, w, h};
        case kMenu: return {left + Width(kL1) + gap, top, w, h};
        case kZoom: return {left + Width(kL1) + gap + Width(kMenu) + gap, top, w, h};
        case kR1: return {right - w, top, w, h};
        case kAccept: return {right - Width(kR1) - gap - w, top, w, h};
        case kBack: return {right - Width(kR1) - gap - Width(kAccept) - gap - w, top, w, h};
        // the directions as two rows of two: language (up, down) over brightness (left, right), the same 6 point gap
        // between them across and down
        // the original options: two rows above the menu's own hints (Back, Exit) along the bottom left; the PC page: one row
        // between the selected setting's note and its Back hint
        case kUp:
        case kDown:
        case kLeft:
        case kRight: {
            const int i = control == kUp ? 0 : control == kDown ? 1 : control == kLeft ? 2 : 3;
            if (mode_ == Mode::PcMenu) {
                return {left + static_cast<float>(i) * (w + pair_gx), pc_row_bottom - h, w, h};
            }
            const float x = left + static_cast<float>(i % 2) * (w + pair_gx);
            return {x, i < 2 ? pair_bottom - 2.0f * h - pair_gy : pair_bottom - h, w, h};
        }
        case kFlipY: return stack(false, 0);
        case kFlipX: return stack(false, 1);
        default: return {0, 2.0f, 0, 0};
        }
    }
    const Slot slot = SlotOf(control);
    // from the edge inward, past the visible controls before this one in its row
    float offset = 0.0f;
    for (int c = 0; c < kControlCount; ++c) {
        const Slot other = SlotOf(c);
        if (c != control && Visible(c) && other.left == slot.left && other.row == slot.row && other.order < slot.order) {
            offset += Width(c) + kGap;
        }
    }
    const float w = Width(control);
    const float x = slot.left ? 0.02f + inset_[0] + offset : 0.98f - inset_[2] - offset - w;
    return {x, 0.03f + inset_[1] + kRowStep * static_cast<float>(slot.row), w, kButtonH};
}

const char* TouchControls::Label(int control) const {
    const bool menu = mode_ != Mode::Gameplay;
    const bool original = mode_ == Mode::OriginalMenu;
    switch (control) {
    case kMenu: return menu ? "Resume" : "Menu";
    case kAccept: return original ? "Settings" : menu ? "OK" : "Use";
    case kBack: return "Back";
    case kZoom: return "Zoom";
    case kUp: return original ? "Language" : "Up";
    case kDown: return original ? "Language" : "Down";
    case kLeft: return original ? "Bright" : "Left";
    case kRight: return original ? "Bright" : "Right";
    case kColumn: return "Column";
    case kFlipY: return "Flip Y";
    case kRun: return "Hurry";
    case kJack: return "Jack";
    case kL1: return "L1";
    case kR1: return "R1";
    default: return "Flip X";
    }
}

void TouchControls::Press(int control, bool down) {
    if (down) {
        ++held_[control];
        until_[control] = SDL_GetTicks() + kMinPressMs;
    } else if (held_[control] > 0) {
        --held_[control];
    }
}

void TouchControls::Release() {
    held_.fill(0);
    until_.fill(0);
    fingers_.fill({});
    move_ = {};
    look_ = {};
}

void TouchControls::SetMode(Mode mode) {
    if (mode == mode_) {
        return;
    }
    Release();
    mode_ = mode;
}

bool TouchControls::ProcessEvent(const SDL_Event& event) {
    if (!joystick_) {
        return false;
    }
    if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        CheckGamepads();
        return false;
    }
    // a real gamepad's button or stick hands the game over to it, a touch hands it back
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && event.gbutton.which != id_) {
        UsePad(true);
        return false;
    }
    if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event.gaxis.which != id_ && std::abs(event.gaxis.value) > 12000) {
        UsePad(true);
        return false;
    }
    if (event.type == SDL_EVENT_FINGER_DOWN) {
        UsePad(false);
    }
    const SDL_FingerID id = event.tfinger.fingerID;
    const float x = event.tfinger.x;
    const float y = event.tfinger.y;
    const auto find = [&]() -> Finger* {
        for (Finger& f : fingers_) {
            if (f.control != -1 && f.id == id) return &f;
        }
        return nullptr;
    };
    const auto deflect = [&](Stick& stick) {
        float dx = (x - stick.cx) * aspect_ / kStickRadius;
        float dy = (y - stick.cy) / kStickRadius;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length > 1.0f) {
            dx /= length;
            dy /= length;
        }
        stick.x = dx;
        stick.y = dy;
    };
    switch (event.type) {
    case SDL_EVENT_FINGER_DOWN: {
        Finger* slot = nullptr;
        for (Finger& f : fingers_) {
            if (f.control == -1) {
                slot = &f;
                break;
            }
        }
        if (!slot) return true;
        for (int c = 0; c < kControlCount; ++c) {
            if (Visible(c) && Layout(c).Contains(x, y)) {
                *slot = {id, c};
                Press(c, true);
                if (c == kRun) run_on_ = !run_on_;
                if (c == kJack) jack_pressed_ = true;
                return true;
            }
        }
        if (mode_ != Mode::Gameplay || y < kStickTop + inset_[1]) return true;
        // the stick appears where the thumb lands, on its own half of the screen
        Stick& stick = x < 0.5f ? move_ : look_;
        if (stick.held) return true;
        stick = {id, true, x, y, 0.0f, 0.0f};
        *slot = {id, kControlCount + (x < 0.5f ? 0 : 1)};
        return true;
    }
    case SDL_EVENT_FINGER_MOTION: {
        Finger* f = find();
        if (f && f->control == kControlCount) deflect(move_);
        if (f && f->control == kControlCount + 1) deflect(look_);
        return true;
    }
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: {
        Finger* f = find();
        if (!f) return true;
        if (f->control < kControlCount) Press(f->control, false);
        else if (f->control == kControlCount) move_ = {};
        else look_ = {};
        *f = {};
        return true;
    }
    default: return false;
    }
}

void TouchControls::Draw(ImDrawList* draw, float width, float height) const {
    if (!joystick_ || pad_active_ || !draw || width <= 0.0f || height <= 0.0f) {
        return;
    }
    aspect_ = width / height;
    UpdateSafeArea();
    const float scale = height / 393.0f;
    for (int c = 0; c < kControlCount; ++c) {
        if (!Visible(c)) continue;
        const Rect r = Layout(c);
        const ImVec2 a(r.x * width, r.y * height);
        const ImVec2 b((r.x + r.w) * width, (r.y + r.h) * height);
        const bool down = held_[c] > 0 || (c == kRun && run_on_);
        draw->AddRectFilled(a, b, down ? IM_COL32(110, 110, 110, 190) : IM_COL32(40, 40, 40, 140), 8.0f * scale);
        draw->AddRect(a, b, IM_COL32(255, 255, 255, down ? 200 : 90), 8.0f * scale, 0, 1.5f * scale);
        const char* label = Label(c);
        const ImVec2 size = ImGui::CalcTextSize(label);
        const int arrow = Arrow(c);
        float text_x = (a.x + b.x - size.x) * 0.5f;
        if (arrow) {
            // the arrow at the left, the label after it
            const float s = (b.y - a.y) * 0.22f;
            const ImVec2 m(a.x + kPadX + s * 1.2f, (a.y + b.y) * 0.5f);
            const ImU32 col = IM_COL32(255, 255, 255, 220);
            const auto tri = [&](float dx, float dy) {  // a triangle pointing along (dx, dy)
                const ImVec2 tip(m.x + dx * s, m.y + dy * s);
                const ImVec2 b1(m.x - dx * s * 0.6f - dy * s * 0.8f, m.y - dy * s * 0.6f + dx * s * 0.8f);
                const ImVec2 b2(m.x - dx * s * 0.6f + dy * s * 0.8f, m.y - dy * s * 0.6f - dx * s * 0.8f);
                draw->AddTriangleFilled(tip, b1, b2, col);
            };
            const auto pair = [&](float dx, float dy) {  // two small triangles pointing apart
                const float k = 0.75f;
                const ImVec2 save = m;
                for (float sign : {1.0f, -1.0f}) {
                    const ImVec2 c0(save.x + sign * dx * s * 0.75f, save.y + sign * dy * s * 0.75f);
                    const ImVec2 tip(c0.x + sign * dx * s * k, c0.y + sign * dy * s * k);
                    const ImVec2 b1(c0.x - dy * s * 0.6f, c0.y + dx * s * 0.6f);
                    const ImVec2 b2(c0.x + dy * s * 0.6f, c0.y - dx * s * 0.6f);
                    draw->AddTriangleFilled(tip, b1, b2, col);
                }
            };
            switch (arrow) {
            case 1: tri(0.0f, -1.0f); break;
            case 2: tri(0.0f, 1.0f); break;
            case 3: tri(-1.0f, 0.0f); break;
            case 4: tri(1.0f, 0.0f); break;
            case 5: pair(0.0f, 1.0f); break;
            default: pair(1.0f, 0.0f); break;
            }
            text_x = m.x + s * 2.0f + 4.0f;
        }
        draw->AddText(ImVec2(text_x, (a.y + b.y - size.y) * 0.5f), IM_COL32(255, 255, 255, 220), label);
    }
    if (mode_ != Mode::Gameplay) {
        return;
    }
    const float radius = kStickRadius * height;
    const auto stick = [&](const Stick& s, float home_x, const char* label) {
        const float safe_x = inset_[0] + home_x * (1.0f - inset_[0] - inset_[2]);
        const float safe_y = std::min(kStickHomeY, 1.0f - inset_[3] - kStickRadius - 0.02f);
        const ImVec2 center(s.held ? s.cx * width : safe_x * width, s.held ? s.cy * height : safe_y * height);
        draw->AddCircleFilled(center, radius, IM_COL32(20, 20, 20, s.held ? 70 : 35), 40);
        draw->AddCircle(center, radius, IM_COL32(255, 255, 255, s.held ? 120 : 50), 40, 1.5f * scale);
        const ImVec2 knob(center.x + s.x * radius, center.y + s.y * radius);
        draw->AddCircleFilled(knob, radius * 0.32f, IM_COL32(255, 255, 255, s.held ? 110 : 40), 24);
        if (!s.held) {
            const ImVec2 size = ImGui::CalcTextSize(label);
            draw->AddText(ImVec2(center.x - size.x * 0.5f, center.y + radius + 4.0f * scale), IM_COL32(255, 255, 255, 70), label);
        }
    };
    stick(move_, kMoveHomeX, "MOVE");
    stick(look_, kLookHomeX, "LOOK");
}

}
