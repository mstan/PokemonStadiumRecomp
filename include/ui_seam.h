#ifndef PKMNSTADIUM_UI_SEAM_H
#define PKMNSTADIUM_UI_SEAM_H

// SS Anne RmlUi launcher (in-app first screen). See src/main/ui_seam.cpp.

#include <string>
#include <utility>
#include <vector>

union SDL_Event;

namespace pkmnstadium::ui_seam {
    // Registers the RmlUi overlay with RT64's render hooks. Call once at
    // startup, before the RT64 application begins presenting frames.
    void install();

    // True while the launcher is the active first screen (before PLAY is
    // clicked). main.cpp uses this to forward input and to hold the game boot.
    bool launcher_visible();

    // True once the user has clicked PLAY (with a valid config). main.cpp's boot
    // thread waits on this before calling recomp::start_game().
    bool play_requested();

    // Forward an SDL event to the launcher (called from the gfx event pump).
    void queue_event(const SDL_Event& ev);

    // ---- controller assignment (captured pre-launch; locked at PLAY) --------

    // Connected gamepads, as (SDL_JoystickID instance id, display name). main.cpp
    // enumerates these after SDL gamecontroller init and hands them to the
    // launcher BEFORE the render hooks initialize, so the dropdowns can show them.
    void set_gamepads(const std::vector<std::pair<int, std::string>>& pads);

    enum class DeviceKind { None = 0, Keyboard = 1, Gamepad = 2 };
    struct PortAssignment {
        bool enabled = false;
        DeviceKind kind = DeviceKind::None;
        int gamepad_instance = -1; // SDL_JoystickID when kind == Gamepad
    };

    // Per N64 port (0..3 == Player 1..4). Valid after play_requested(); the boot
    // thread reads it to wire input routing and Transfer Pak presence.
    PortAssignment port_assignment(int port);

    // Resolve whether the app should open directly in fullscreen. Reads the
    // PSR_WINDOW_MODE / PSR_FULLSCREEN environment overrides first, then the
    // persisted `window_mode` (or `fullscreen`) key in launcher.cfg; defaults
    // to windowed. main.cpp calls this before the renderer is created so users
    // can launch fullscreen without pressing Alt+Enter (issue #18). Also seeds
    // the launcher's in-memory preference so it round-trips through the cfg.
    bool startup_fullscreen();

    // ---- Settings page (Display / Graphics / Audio) -------------------------
    // Resolve the launch-time value of each setting BEFORE the renderer/audio are
    // created. Precedence mirrors startup_fullscreen: env override > launcher.cfg
    // > built-in default. Called from main.cpp (graphics API, audio device) and
    // rt64_render_context.cpp (supersampling, MSAA).

    // "auto" | "vulkan" | "d3d12".
    std::string startup_graphics_api();

    // 3D supersampling depth as the GraphicsConfig ds_option (downsample
    // multiplier): 1 (off), 2, or 4. set_application_user_config derives the
    // render resolution as 3x*ds and downsamples by ds, so the presented image
    // stays at the native 3x window scale that keeps the 2D menus correct (#12);
    // only the SSAA depth changes. Driving it through GraphicsConfig.ds_option
    // lets the launcher apply it live (update_config) as well as at startup.
    int startup_ds_option();

    // MSAA sample count: 0 (off), 2, 4, or 8. Drives GraphicsConfig.msaa_option.
    int startup_msaa();

    // Audio output device name substring, or empty for the system default.
    std::string startup_audio_device();

    // Hand the launcher the list of audio output device display names (for the
    // Settings audio dropdown). Called before the render hooks initialize.
    void set_audio_devices(const std::vector<std::string>& devices);
}

#endif // PKMNSTADIUM_UI_SEAM_H
