#ifndef PKMNSTADIUM_RECOMPUI_LAUNCHER_H
#define PKMNSTADIUM_RECOMPUI_LAUNCHER_H

// recompui_launcher.h — PSR host glue for the shared recomp-ui pre-boot
// launcher (Dear ImGui). Replaces the old RmlUi-over-RT64 overlay (ui_seam):
// the launcher now runs in its OWN SDL/GL window BEFORE the game's RT64 render
// context is created, then tears that window down and boots the game.
//
// See src/main/recompui_launcher.cpp. The recomp-ui C ABI it wraps lives in
// recomp-ui/src/recomp_launcher.h.

#include <cstddef>
#include <string>

namespace pkmnstadium::recompui {

// Per-port input device kind (mirrors the recomp-ui player_src vocabulary).
enum class DeviceKind { None = 0, Keyboard = 1, Gamepad = 2 };

struct PortAssignment {
    bool enabled = false;                 // port responds (has a device or a Transfer Pak)
    DeviceKind kind = DeviceKind::None;   // input source
    int gamepad_instance = -1;            // SDL_JoystickID when kind == Gamepad, else -1
};

// Run the pre-boot launcher window. Blocks until the user launches or quits.
//   returns true  -> boot the game; out_rom holds the chosen N64 ROM path
//                    (possibly == rom_path_in). Persists launcher.cfg.
//   returns false -> QUIT; the caller should exit cleanly.
// An UNAVAILABLE result (assets/GL init failed) is treated as "boot as if the
// launcher was skipped": returns true with out_rom = rom_path_in. rom_path_in
// may be null/empty when no ROM has been chosen yet.
bool run(const char* rom_path_in, char* out_rom, std::size_t out_len);

// Per N64 port (0..3 == Player 1..4). Valid after a true-returning run().
PortAssignment port_assignment(int port);

// Launch-time settings, resolved from launcher.cfg (+ PSR_* env overrides),
// mirroring the old ui_seam getters. Safe to call after run().
bool        startup_fullscreen();     // window_mode / PSR_WINDOW_MODE / PSR_FULLSCREEN
std::string startup_graphics_api();   // "auto" | "vulkan" | "d3d12"
int         startup_ds_option();      // supersampling depth: 1 | 2 | 4
int         startup_msaa();           // MSAA sample count: 0 | 2 | 4 | 8
std::string startup_audio_device();   // device-name substring; "" = system default

} // namespace pkmnstadium::recompui

#endif // PKMNSTADIUM_RECOMPUI_LAUNCHER_H
