#ifndef PKMNSTADIUM_INPUT_BINDINGS_H
#define PKMNSTADIUM_INPUT_BINDINGS_H

// Per-device-type input binding model for PokemonStadiumRecomp.
//
// This is a deliberately small, self-contained binding layer — NOT a port of
// Zelda64Recomp's recompui input/config stack (which drags in gyro, mouse,
// background-input modes, and the mod-menu context system PSR does not use).
//
// Two binding tables exist: one for the Keyboard device type and one for the
// Controller device type. Bindings are keyed per device *type*, not per port,
// so all gamepads share the controller layout and all keyboards share the
// keyboard layout. Each N64 input has up to `bindings_per_input` alternate
// bindings (e.g. N64 Z defaults to BOTH shoulder triggers).
//
// main.cpp's get_n64_input() consults these tables every poll via accumulate();
// the SS Anne launcher (ui_seam.cpp) edits them via get/set_binding + the scan
// API and persists them to input.cfg.
//
// See src/main/input_bindings.cpp.

#include <cstdint>
#include <filesystem>
#include <string>

// One SDL forward-declaration so this header stays SDL-light. SDL.h (included
// by every translation unit that uses accumulate()) defines this identically;
// duplicate identical typedefs are well-formed in C++.
struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;
union SDL_Event;

namespace pkmnstadium::input {

// What a single binding refers to. `id` is interpreted per type:
//   Key        -> SDL_Scancode
//   PadButton  -> SDL_GameControllerButton
//   PadAxisPos -> SDL_GameControllerAxis, active/positive when value >  +threshold
//   PadAxisNeg -> SDL_GameControllerAxis, active/negative when value <  -threshold
enum class FieldType : uint32_t {
    None       = 0,
    Key        = 1,
    PadButton  = 2,
    PadAxisPos = 3,
    PadAxisNeg = 4,
};

struct InputField {
    uint32_t type = static_cast<uint32_t>(FieldType::None);
    int32_t  id   = -1;
    bool empty() const { return type == static_cast<uint32_t>(FieldType::None); }
};

// Every N64 input we can bind. The digital buttons carry an N64 contStat bit
// (see n64_bit); the four STICK_* directions feed the analog stick instead.
enum class N64Input : int {
    A = 0, B, Z, START,
    DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT,
    L, R,
    C_UP, C_DOWN, C_LEFT, C_RIGHT,
    STICK_UP, STICK_DOWN, STICK_LEFT, STICK_RIGHT,
    COUNT
};

enum class Device : int {
    Keyboard   = 0,
    Controller = 1,
    COUNT
};

constexpr int bindings_per_input = 2;

// N64 contStat button bit for an input, or 0 for the analog STICK_* directions.
uint16_t n64_bit(N64Input in);
bool     is_stick(N64Input in);

// Short stable identifier used in input.cfg (e.g. "A", "C_UP", "STICK_LEFT").
const char* input_key(N64Input in);
// Human label for the rebind UI (e.g. "A", "C-Up", "Stick Left").
const char* input_label(N64Input in);

// Table accessors. idx in [0, bindings_per_input).
const InputField& get_binding(Device dev, N64Input in, int idx);
void              set_binding(Device dev, N64Input in, int idx, InputField field);

// Reset both tables to the built-in defaults (which reproduce PSR's historical
// hardcoded layout exactly, so a fresh build with no input.cfg is unchanged).
void reset_defaults();

// Human-readable name of a bound field for the UI (e.g. "X", "Button A",
// "LT", "R-Stick Up"). Returns "—" for an empty field.
std::string field_to_string(const InputField& field);

// Persistence. Both read/write exe_dir()/input.cfg. load() falls back to
// defaults for any input not present (and seeds defaults entirely if the file
// is absent). save() is called after every rebind.
void load();
void save();

// ---- runtime evaluation (called from get_n64_input) -----------------------

// OR the N64 buttons produced by the active bindings into *buttons and add the
// analog-stick contribution into *lx/*ly (raw, pre-deadzone, signed). The
// keyboard table is consulted iff `ks` is non-null; the controller table iff
// `pad` is non-null — so a port driven by both (legacy single-human mode) sees
// the union, exactly as before.
void accumulate(SDL_GameController* pad, const uint8_t* ks,
                uint16_t* buttons, int32_t* lx, int32_t* ly);

// ---- rebind scan flow ------------------------------------------------------

// Arm a scan: the next relevant input event for `dev` (a key press for
// Keyboard; a button press or axis throw for Controller) is captured and
// written to (in, idx), then the scan disarms. Feeding events is done by the
// launcher's event pump via handle_scan_event().
void start_scan(Device dev, N64Input in, int idx);
void cancel_scan();
bool scanning();

// Offer an SDL event to an armed scan. Returns true if it captured the binding
// (the caller should then refresh the rebind UI and call save()). Returns false
// if no scan is armed or the event was not a bindable input for the scanned
// device. ESC cancels the scan and returns true (with no binding change).
bool handle_scan_event(const SDL_Event& ev);

} // namespace pkmnstadium::input

#endif // PKMNSTADIUM_INPUT_BINDINGS_H
