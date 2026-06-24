/*
 * input_bindings.cpp — per-device-type input binding model.
 *
 * A small, self-contained binding layer (see include/input_bindings.h for the
 * rationale and the "no Zelda recompui stack" constraint). Holds two binding
 * tables (Keyboard, Controller), evaluates them into N64 controller state for
 * get_n64_input(), and persists them to input.cfg.
 *
 * The built-in defaults reproduce PSR's historical hardcoded layout exactly, so
 * a fresh build with no input.cfg behaves identically to before this layer
 * existed.
 *
 * Copyright (c) 2026 Matthew Stanley
 */

#include "input_bindings.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>

#include <SDL.h>

#include "app_paths.h"

namespace pkmnstadium::input {

namespace {

constexpr int kDeviceCount = static_cast<int>(Device::COUNT);
constexpr int kInputCount  = static_cast<int>(N64Input::COUNT);

// The whole binding table is read every poll by accumulate() on the input
// thread and mutated by the launcher on the render thread, so guard it.
std::mutex g_mutex;
InputField g_bindings[kDeviceCount][kInputCount][bindings_per_input];

// ---- scan state ------------------------------------------------------------
struct ScanState {
    bool     armed = false;
    Device   dev   = Device::Keyboard;
    N64Input in    = N64Input::A;
    int      idx   = 0;
};
ScanState g_scan;

// N64 contStat button bits. STICK_* directions have no bit (they feed analog).
struct InputMeta {
    uint16_t    bit;
    bool        stick;
    const char* key;    // input.cfg identifier
    const char* label;  // UI label
};

const InputMeta& meta(N64Input in) {
    static const InputMeta table[kInputCount] = {
        /* A          */ {0x8000, false, "A",           "A"},
        /* B          */ {0x4000, false, "B",           "B"},
        /* Z          */ {0x2000, false, "Z",           "Z"},
        /* START      */ {0x1000, false, "START",       "Start"},
        /* DPAD_UP    */ {0x0800, false, "DPAD_UP",     "D-Pad Up"},
        /* DPAD_DOWN  */ {0x0400, false, "DPAD_DOWN",   "D-Pad Down"},
        /* DPAD_LEFT  */ {0x0200, false, "DPAD_LEFT",   "D-Pad Left"},
        /* DPAD_RIGHT */ {0x0100, false, "DPAD_RIGHT",  "D-Pad Right"},
        /* L          */ {0x0020, false, "L",           "L"},
        /* R          */ {0x0010, false, "R",           "R"},
        /* C_UP       */ {0x0008, false, "C_UP",        "C-Up"},
        /* C_DOWN     */ {0x0004, false, "C_DOWN",      "C-Down"},
        /* C_LEFT     */ {0x0002, false, "C_LEFT",      "C-Left"},
        /* C_RIGHT    */ {0x0001, false, "C_RIGHT",     "C-Right"},
        /* STICK_UP   */ {0x0000, true,  "STICK_UP",    "Stick Up"},
        /* STICK_DOWN */ {0x0000, true,  "STICK_DOWN",  "Stick Down"},
        /* STICK_LEFT */ {0x0000, true,  "STICK_LEFT",  "Stick Left"},
        /* STICK_RIGHT*/ {0x0000, true,  "STICK_RIGHT", "Stick Right"},
    };
    return table[static_cast<int>(in)];
}

InputField key_field(SDL_Scancode sc) {
    return {static_cast<uint32_t>(FieldType::Key), static_cast<int32_t>(sc)};
}
InputField btn_field(SDL_GameControllerButton b) {
    return {static_cast<uint32_t>(FieldType::PadButton), static_cast<int32_t>(b)};
}
InputField axis_pos(SDL_GameControllerAxis a) {
    return {static_cast<uint32_t>(FieldType::PadAxisPos), static_cast<int32_t>(a)};
}
InputField axis_neg(SDL_GameControllerAxis a) {
    return {static_cast<uint32_t>(FieldType::PadAxisNeg), static_cast<int32_t>(a)};
}
// Raw SDL_Joystick fields (issue #15): id is the raw joystick button/axis index.
InputField joy_btn_field(int raw_button) {
    return {static_cast<uint32_t>(FieldType::JoyButton), static_cast<int32_t>(raw_button)};
}
InputField joy_axis_pos(int raw_axis) {
    return {static_cast<uint32_t>(FieldType::JoyAxisPos), static_cast<int32_t>(raw_axis)};
}
InputField joy_axis_neg(int raw_axis) {
    return {static_cast<uint32_t>(FieldType::JoyAxisNeg), static_cast<int32_t>(raw_axis)};
}
// Raw joystick axis "pressed" threshold. The 8BitDo 64's C-buttons report as a
// digital throw to the axis extreme, so a firm mid threshold reads cleanly while
// rejecting resting drift.
constexpr int kRawAxisThreshold = 16000;

void set_default(Device dev, N64Input in, InputField a, InputField b = {}) {
    g_bindings[static_cast<int>(dev)][static_cast<int>(in)][0] = a;
    g_bindings[static_cast<int>(dev)][static_cast<int>(in)][1] = b;
}

// Caller must hold g_mutex.
void reset_defaults_locked() {
    for (auto& dev : g_bindings)
        for (auto& in : dev)
            for (auto& f : in) f = InputField{};

    // Keyboard — the historical Project64/Mupen-style layout from get_n64_input.
    set_default(Device::Keyboard, N64Input::A,          key_field(SDL_SCANCODE_X));
    set_default(Device::Keyboard, N64Input::B,          key_field(SDL_SCANCODE_Z));
    set_default(Device::Keyboard, N64Input::Z,          key_field(SDL_SCANCODE_LSHIFT), key_field(SDL_SCANCODE_SPACE));
    set_default(Device::Keyboard, N64Input::START,      key_field(SDL_SCANCODE_RETURN), key_field(SDL_SCANCODE_KP_ENTER));
    set_default(Device::Keyboard, N64Input::DPAD_UP,    key_field(SDL_SCANCODE_UP));
    set_default(Device::Keyboard, N64Input::DPAD_DOWN,  key_field(SDL_SCANCODE_DOWN));
    set_default(Device::Keyboard, N64Input::DPAD_LEFT,  key_field(SDL_SCANCODE_LEFT));
    set_default(Device::Keyboard, N64Input::DPAD_RIGHT, key_field(SDL_SCANCODE_RIGHT));
    set_default(Device::Keyboard, N64Input::L,          key_field(SDL_SCANCODE_Q));
    set_default(Device::Keyboard, N64Input::R,          key_field(SDL_SCANCODE_E));
    set_default(Device::Keyboard, N64Input::C_UP,       key_field(SDL_SCANCODE_I));
    set_default(Device::Keyboard, N64Input::C_DOWN,     key_field(SDL_SCANCODE_K));
    set_default(Device::Keyboard, N64Input::C_LEFT,     key_field(SDL_SCANCODE_J));
    set_default(Device::Keyboard, N64Input::C_RIGHT,    key_field(SDL_SCANCODE_L));
    set_default(Device::Keyboard, N64Input::STICK_UP,   key_field(SDL_SCANCODE_W));
    set_default(Device::Keyboard, N64Input::STICK_DOWN, key_field(SDL_SCANCODE_S));
    set_default(Device::Keyboard, N64Input::STICK_LEFT, key_field(SDL_SCANCODE_A));
    set_default(Device::Keyboard, N64Input::STICK_RIGHT,key_field(SDL_SCANCODE_D));

    // Controller — bumpers for L/R, triggers for Z, right stick for C, left
    // stick for analog (matches the issue-#8 fix in get_n64_input).
    set_default(Device::Controller, N64Input::A,          btn_field(SDL_CONTROLLER_BUTTON_A));
    set_default(Device::Controller, N64Input::B,          btn_field(SDL_CONTROLLER_BUTTON_B));
    set_default(Device::Controller, N64Input::Z,          axis_pos(SDL_CONTROLLER_AXIS_TRIGGERLEFT), axis_pos(SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
    set_default(Device::Controller, N64Input::START,      btn_field(SDL_CONTROLLER_BUTTON_START));
    set_default(Device::Controller, N64Input::DPAD_UP,    btn_field(SDL_CONTROLLER_BUTTON_DPAD_UP));
    set_default(Device::Controller, N64Input::DPAD_DOWN,  btn_field(SDL_CONTROLLER_BUTTON_DPAD_DOWN));
    set_default(Device::Controller, N64Input::DPAD_LEFT,  btn_field(SDL_CONTROLLER_BUTTON_DPAD_LEFT));
    set_default(Device::Controller, N64Input::DPAD_RIGHT, btn_field(SDL_CONTROLLER_BUTTON_DPAD_RIGHT));
    set_default(Device::Controller, N64Input::L,          btn_field(SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
    set_default(Device::Controller, N64Input::R,          btn_field(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
    set_default(Device::Controller, N64Input::C_UP,       axis_neg(SDL_CONTROLLER_AXIS_RIGHTY));
    set_default(Device::Controller, N64Input::C_DOWN,     axis_pos(SDL_CONTROLLER_AXIS_RIGHTY));
    set_default(Device::Controller, N64Input::C_LEFT,     axis_neg(SDL_CONTROLLER_AXIS_RIGHTX));
    set_default(Device::Controller, N64Input::C_RIGHT,    axis_pos(SDL_CONTROLLER_AXIS_RIGHTX));
    set_default(Device::Controller, N64Input::STICK_UP,   axis_neg(SDL_CONTROLLER_AXIS_LEFTY));
    set_default(Device::Controller, N64Input::STICK_DOWN, axis_pos(SDL_CONTROLLER_AXIS_LEFTY));
    set_default(Device::Controller, N64Input::STICK_LEFT, axis_neg(SDL_CONTROLLER_AXIS_LEFTX));
    set_default(Device::Controller, N64Input::STICK_RIGHT,axis_pos(SDL_CONTROLLER_AXIS_LEFTX));
}

// Activation threshold for an axis acting as a digital input. Triggers rest at
// 0 and read up to 32767, so a half-press (0x4000) trips Z; stick axes use the
// historical C-button deadzone of 8000.
int axis_threshold(int axis) {
    if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        return 0x4000;
    return 8000;
}

bool field_active_digital(const InputField& f, SDL_GameController* pad, const uint8_t* ks) {
    switch (static_cast<FieldType>(f.type)) {
        case FieldType::Key:
            return ks && f.id >= 0 && f.id < SDL_NUM_SCANCODES && ks[f.id] != 0;
        case FieldType::PadButton:
            return pad && SDL_GameControllerGetButton(pad, static_cast<SDL_GameControllerButton>(f.id));
        case FieldType::PadAxisPos:
            return pad && SDL_GameControllerGetAxis(pad, static_cast<SDL_GameControllerAxis>(f.id)) >  axis_threshold(f.id);
        case FieldType::PadAxisNeg:
            return pad && SDL_GameControllerGetAxis(pad, static_cast<SDL_GameControllerAxis>(f.id)) < -axis_threshold(f.id);
        case FieldType::JoyButton: {
            SDL_Joystick* j = pad ? SDL_GameControllerGetJoystick(pad) : nullptr;
            return j && f.id >= 0 && f.id < SDL_JoystickNumButtons(j) && SDL_JoystickGetButton(j, f.id);
        }
        case FieldType::JoyAxisPos: {
            SDL_Joystick* j = pad ? SDL_GameControllerGetJoystick(pad) : nullptr;
            return j && f.id >= 0 && f.id < SDL_JoystickNumAxes(j) && SDL_JoystickGetAxis(j, f.id) >  kRawAxisThreshold;
        }
        case FieldType::JoyAxisNeg: {
            SDL_Joystick* j = pad ? SDL_GameControllerGetJoystick(pad) : nullptr;
            return j && f.id >= 0 && f.id < SDL_JoystickNumAxes(j) && SDL_JoystickGetAxis(j, f.id) < -kRawAxisThreshold;
        }
        default:
            return false;
    }
}

// Magnitude (0..32767) this field contributes toward its stick direction. Pad
// axes contribute proportionally (raw value), keys/buttons contribute full
// deflection — matching the historical WASD-and-left-stick behavior.
int field_stick_contribution(const InputField& f, SDL_GameController* pad, const uint8_t* ks) {
    switch (static_cast<FieldType>(f.type)) {
        case FieldType::Key:
            return (ks && f.id >= 0 && f.id < SDL_NUM_SCANCODES && ks[f.id]) ? 32767 : 0;
        case FieldType::PadButton:
            return (pad && SDL_GameControllerGetButton(pad, static_cast<SDL_GameControllerButton>(f.id))) ? 32767 : 0;
        case FieldType::PadAxisPos: {
            if (!pad) return 0;
            int v = SDL_GameControllerGetAxis(pad, static_cast<SDL_GameControllerAxis>(f.id));
            return v > 0 ? v : 0;
        }
        case FieldType::PadAxisNeg: {
            if (!pad) return 0;
            int v = SDL_GameControllerGetAxis(pad, static_cast<SDL_GameControllerAxis>(f.id));
            return v < 0 ? -v : 0;
        }
        case FieldType::JoyButton: {
            SDL_Joystick* j = pad ? SDL_GameControllerGetJoystick(pad) : nullptr;
            return (j && f.id >= 0 && f.id < SDL_JoystickNumButtons(j) && SDL_JoystickGetButton(j, f.id)) ? 32767 : 0;
        }
        case FieldType::JoyAxisPos: {
            SDL_Joystick* j = pad ? SDL_GameControllerGetJoystick(pad) : nullptr;
            if (!j || f.id < 0 || f.id >= SDL_JoystickNumAxes(j)) return 0;
            int v = SDL_JoystickGetAxis(j, f.id);
            return v > 0 ? v : 0;
        }
        case FieldType::JoyAxisNeg: {
            SDL_Joystick* j = pad ? SDL_GameControllerGetJoystick(pad) : nullptr;
            if (!j || f.id < 0 || f.id >= SDL_JoystickNumAxes(j)) return 0;
            int v = SDL_JoystickGetAxis(j, f.id);
            return v < 0 ? -v : 0;
        }
        default:
            return 0;
    }
}

// ---- input.cfg serialization ----------------------------------------------

const char* device_key(Device d) { return d == Device::Keyboard ? "kb" : "pad"; }

std::string encode_field(const InputField& f) {
    switch (static_cast<FieldType>(f.type)) {
        case FieldType::Key:        return "key:"   + std::to_string(f.id);
        case FieldType::PadButton:  return "button:"+ std::to_string(f.id);
        case FieldType::PadAxisPos: return "axis+:" + std::to_string(f.id);
        case FieldType::PadAxisNeg: return "axis-:" + std::to_string(f.id);
        case FieldType::JoyButton:  return "joybtn:"  + std::to_string(f.id);
        case FieldType::JoyAxisPos: return "joyaxis+:"+ std::to_string(f.id);
        case FieldType::JoyAxisNeg: return "joyaxis-:"+ std::to_string(f.id);
        default:                    return "none";
    }
}

bool decode_field(const std::string& s, InputField* out) {
    auto colon = s.find(':');
    if (colon == std::string::npos) return false;
    std::string type = s.substr(0, colon);
    int id = 0;
    try { id = std::stoi(s.substr(colon + 1)); } catch (...) { return false; }
    if      (type == "key")      out->type = static_cast<uint32_t>(FieldType::Key);
    else if (type == "button")   out->type = static_cast<uint32_t>(FieldType::PadButton);
    else if (type == "axis+")    out->type = static_cast<uint32_t>(FieldType::PadAxisPos);
    else if (type == "axis-")    out->type = static_cast<uint32_t>(FieldType::PadAxisNeg);
    else if (type == "joybtn")   out->type = static_cast<uint32_t>(FieldType::JoyButton);
    else if (type == "joyaxis+") out->type = static_cast<uint32_t>(FieldType::JoyAxisPos);
    else if (type == "joyaxis-") out->type = static_cast<uint32_t>(FieldType::JoyAxisNeg);
    else return false;
    out->id = id;
    return true;
}

N64Input input_from_key(const std::string& k, bool* ok) {
    for (int i = 0; i < kInputCount; ++i) {
        if (k == meta(static_cast<N64Input>(i)).key) { *ok = true; return static_cast<N64Input>(i); }
    }
    *ok = false;
    return N64Input::A;
}

const char* button_label(int b) {
    switch (b) {
        case SDL_CONTROLLER_BUTTON_A:             return "A";
        case SDL_CONTROLLER_BUTTON_B:             return "B";
        case SDL_CONTROLLER_BUTTON_X:             return "X";
        case SDL_CONTROLLER_BUTTON_Y:             return "Y";
        case SDL_CONTROLLER_BUTTON_BACK:          return "Back";
        case SDL_CONTROLLER_BUTTON_GUIDE:         return "Guide";
        case SDL_CONTROLLER_BUTTON_START:         return "Start";
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:     return "L3";
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    return "R3";
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return "LB";
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RB";
        case SDL_CONTROLLER_BUTTON_DPAD_UP:       return "D-Pad Up";
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return "D-Pad Down";
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return "D-Pad Left";
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return "D-Pad Right";
        default:                                  return "Button";
    }
}

std::string axis_label(int axis, bool positive) {
    switch (axis) {
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:  return "LT";
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return "RT";
        case SDL_CONTROLLER_AXIS_LEFTX:        return positive ? "L-Stick Right" : "L-Stick Left";
        case SDL_CONTROLLER_AXIS_LEFTY:        return positive ? "L-Stick Down"  : "L-Stick Up";
        case SDL_CONTROLLER_AXIS_RIGHTX:       return positive ? "R-Stick Right" : "R-Stick Left";
        case SDL_CONTROLLER_AXIS_RIGHTY:       return positive ? "R-Stick Down"  : "R-Stick Up";
        default:                               return positive ? "Axis+" : "Axis-";
    }
}

} // namespace

uint16_t n64_bit(N64Input in) { return meta(in).bit; }
bool     is_stick(N64Input in) { return meta(in).stick; }
const char* input_key(N64Input in)   { return meta(in).key; }
const char* input_label(N64Input in) { return meta(in).label; }

const InputField& get_binding(Device dev, N64Input in, int idx) {
    std::scoped_lock lk(g_mutex);
    return g_bindings[static_cast<int>(dev)][static_cast<int>(in)][idx];
}

void set_binding(Device dev, N64Input in, int idx, InputField field) {
    std::scoped_lock lk(g_mutex);
    g_bindings[static_cast<int>(dev)][static_cast<int>(in)][idx] = field;
}

void reset_defaults() {
    std::scoped_lock lk(g_mutex);
    reset_defaults_locked();
}

std::string field_to_string(const InputField& field) {
    switch (static_cast<FieldType>(field.type)) {
        case FieldType::Key: {
            const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(field.id));
            return (name && name[0]) ? std::string(name) : ("Key " + std::to_string(field.id));
        }
        case FieldType::PadButton:  return button_label(field.id);
        case FieldType::PadAxisPos: return axis_label(field.id, true);
        case FieldType::PadAxisNeg: return axis_label(field.id, false);
        case FieldType::JoyButton:  return "Btn " + std::to_string(field.id);
        case FieldType::JoyAxisPos: return "Axis " + std::to_string(field.id) + "+";
        case FieldType::JoyAxisNeg: return "Axis " + std::to_string(field.id) + "-";
        default:                    return "\xE2\x80\x94"; // em dash
    }
}

void load() {
    std::scoped_lock lk(g_mutex);
    reset_defaults_locked(); // baseline; cfg lines override individual bindings

    const std::filesystem::path path = pkmnstadium::exe_dir() / "input.cfg";
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "[input] no input.cfg -> using default bindings\n");
        return;
    }
    std::string line;
    while (std::getline(f, line)) {
        // Trim trailing CR (Windows line endings) and skip comments/blanks.
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string lhs = line.substr(0, eq);
        std::string rhs = line.substr(eq + 1);

        // lhs = <dev>.<input>.<idx>
        auto d1 = lhs.find('.');
        auto d2 = lhs.rfind('.');
        if (d1 == std::string::npos || d2 == d1) continue;
        std::string dev_s = lhs.substr(0, d1);
        std::string in_s  = lhs.substr(d1 + 1, d2 - d1 - 1);
        std::string idx_s = lhs.substr(d2 + 1);

        Device dev;
        if      (dev_s == "kb")  dev = Device::Keyboard;
        else if (dev_s == "pad") dev = Device::Controller;
        else continue;

        bool ok = false;
        N64Input in = input_from_key(in_s, &ok);
        if (!ok) continue;

        int idx = 0;
        try { idx = std::stoi(idx_s); } catch (...) { continue; }
        if (idx < 0 || idx >= bindings_per_input) continue;

        InputField field{};
        if (rhs == "none") {
            field = InputField{};
        } else if (!decode_field(rhs, &field)) {
            continue;
        }
        // First cfg line for a (dev,input) replaces the default pair; subsequent
        // lines (idx 1) fill the alternate slot. Because we keep the default as a
        // baseline, an input entirely absent from the cfg keeps its default.
        g_bindings[static_cast<int>(dev)][static_cast<int>(in)][idx] = field;
        // A bound idx 0 with no idx 1 line should clear the stale default alt.
        if (idx == 0) g_bindings[static_cast<int>(dev)][static_cast<int>(in)][1] = InputField{};
    }
    std::fprintf(stderr, "[input] loaded bindings from input.cfg\n");
}

void save() {
    std::scoped_lock lk(g_mutex);
    const std::filesystem::path path = pkmnstadium::exe_dir() / "input.cfg";
    std::ofstream f(path, std::ios::trunc);
    if (!f) {
        std::fprintf(stderr, "[input] WARN: cannot write input.cfg\n");
        return;
    }
    f << "# PokemonStadiumRecomp input bindings - managed by the launcher's\n";
    f << "# per-controller rebinding menu. Format: <dev>.<input>.<idx>=<field>\n";
    f << "#   dev:   kb (keyboard) | pad (controller, shared by all gamepads)\n";
    f << "#   field: key:<scancode> | button:<n> | axis+:<n> | axis-:<n> | none\n";
    for (int d = 0; d < kDeviceCount; ++d) {
        for (int i = 0; i < kInputCount; ++i) {
            for (int b = 0; b < bindings_per_input; ++b) {
                const InputField& fld = g_bindings[d][i][b];
                if (fld.empty()) continue;
                f << device_key(static_cast<Device>(d)) << '.'
                  << meta(static_cast<N64Input>(i)).key << '.' << b
                  << '=' << encode_field(fld) << '\n';
            }
        }
    }
}

void accumulate(SDL_GameController* pad, const uint8_t* ks,
                uint16_t* buttons, int32_t* lx, int32_t* ly) {
    std::scoped_lock lk(g_mutex);

    int up = 0, down = 0, left = 0, right = 0;

    auto eval = [&](Device dev, SDL_GameController* p, const uint8_t* k) {
        const int di = static_cast<int>(dev);
        for (int i = 0; i < kInputCount; ++i) {
            const N64Input in = static_cast<N64Input>(i);
            const bool stick = meta(in).stick;
            for (int b = 0; b < bindings_per_input; ++b) {
                const InputField& f = g_bindings[di][i][b];
                if (f.empty()) continue;
                if (stick) {
                    int c = field_stick_contribution(f, p, k);
                    if (c <= 0) continue;
                    switch (in) {
                        case N64Input::STICK_UP:    up    += c; break;
                        case N64Input::STICK_DOWN:  down  += c; break;
                        case N64Input::STICK_LEFT:  left  += c; break;
                        case N64Input::STICK_RIGHT: right += c; break;
                        default: break;
                    }
                } else if (field_active_digital(f, p, k)) {
                    *buttons |= meta(in).bit;
                }
            }
        }
    };

    if (ks)  eval(Device::Keyboard,   nullptr, ks);   // keyboard table: keys only
    if (pad) eval(Device::Controller, pad,     nullptr); // controller table: pad only

    // SDL stick Y is negative-up; the caller negates y at the end, so positive
    // ly means "down". Match the historical (LEFTY + WASD) summation.
    *lx += right - left;
    *ly += down - up;
}

// ---- scan flow -------------------------------------------------------------

void start_scan(Device dev, N64Input in, int idx) {
    std::scoped_lock lk(g_mutex);
    g_scan = ScanState{true, dev, in, idx};
}

void cancel_scan() {
    std::scoped_lock lk(g_mutex);
    g_scan.armed = false;
}

bool scanning() {
    std::scoped_lock lk(g_mutex);
    return g_scan.armed;
}

// Is this raw joystick button/axis already part of the controller's SDL
// GameController mapping? If so, the scan should prefer the (cleaner, portable)
// SDL_CONTROLLER* event and ignore the raw one — raw capture is reserved for
// inputs the mapping can't express (issue #15: 8BitDo 64 C-buttons).
static bool raw_input_is_mapped(SDL_GameController* gc, bool is_axis, int raw_index) {
    if (!gc) return false;
    auto hit = [&](SDL_GameControllerButtonBind b) {
        if (!is_axis && b.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON) return b.value.button == raw_index;
        if ( is_axis && b.bindType == SDL_CONTROLLER_BINDTYPE_AXIS)   return b.value.axis   == raw_index;
        return false;
    };
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
        if (hit(SDL_GameControllerGetBindForButton(gc, static_cast<SDL_GameControllerButton>(i)))) return true;
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i)
        if (hit(SDL_GameControllerGetBindForAxis(gc, static_cast<SDL_GameControllerAxis>(i)))) return true;
    return false;
}

bool handle_scan_event(const SDL_Event& ev) {
    ScanState s;
    {
        std::scoped_lock lk(g_mutex);
        if (!g_scan.armed) return false;
        s = g_scan;
    }

    InputField captured{};
    bool got = false;

    // ESC cancels regardless of which device we're scanning.
    if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        cancel_scan();
        return true;
    }

    if (s.dev == Device::Keyboard) {
        if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
            captured = key_field(static_cast<SDL_Scancode>(ev.key.keysym.scancode));
            got = true;
        }
    } else { // Controller
        if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
            captured = btn_field(static_cast<SDL_GameControllerButton>(ev.cbutton.button));
            got = true;
        } else if (ev.type == SDL_CONTROLLERAXISMOTION) {
            // Require a decisive throw so resting drift doesn't capture.
            constexpr int kScanThreshold = 20000;
            if (ev.caxis.value > kScanThreshold) {
                captured = axis_pos(static_cast<SDL_GameControllerAxis>(ev.caxis.axis));
                got = true;
            } else if (ev.caxis.value < -kScanThreshold) {
                captured = axis_neg(static_cast<SDL_GameControllerAxis>(ev.caxis.axis));
                got = true;
            }
        } else if (ev.type == SDL_JOYBUTTONDOWN) {
            // Issue #15: raw fallback for inputs SDL_GameController can't express
            // (e.g. 8BitDo 64 C-buttons on buttons 16/17). Diagnostic log surfaces
            // the raw index in the user's log so we can set sane defaults later.
            std::fprintf(stderr, "[input] scan: joy button %d (instance %d)\n",
                         (int)ev.jbutton.button, (int)ev.jbutton.which);
            SDL_GameController* gc = SDL_GameControllerFromInstanceID(ev.jbutton.which);
            if (!raw_input_is_mapped(gc, /*is_axis=*/false, ev.jbutton.button)) {
                captured = joy_btn_field(ev.jbutton.button);
                got = true;
            }
        } else if (ev.type == SDL_JOYAXISMOTION) {
            constexpr int kScanThreshold = 20000;
            if (ev.jaxis.value > kScanThreshold || ev.jaxis.value < -kScanThreshold) {
                std::fprintf(stderr, "[input] scan: joy axis %d = %d (instance %d)\n",
                             (int)ev.jaxis.axis, (int)ev.jaxis.value, (int)ev.jaxis.which);
                SDL_GameController* gc = SDL_GameControllerFromInstanceID(ev.jaxis.which);
                if (!raw_input_is_mapped(gc, /*is_axis=*/true, ev.jaxis.axis)) {
                    captured = (ev.jaxis.value > 0) ? joy_axis_pos(ev.jaxis.axis)
                                                    : joy_axis_neg(ev.jaxis.axis);
                    got = true;
                }
            }
        }
    }

    if (!got) return false;

    set_binding(s.dev, s.in, s.idx, captured);
    cancel_scan();
    save();
    return true;
}

} // namespace pkmnstadium::input
