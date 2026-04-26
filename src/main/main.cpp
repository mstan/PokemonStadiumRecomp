/*
 * main.cpp — PokemonStadiumRecomp runner entry point.
 *
 * Adapted from Zelda64Recomp's src/main/main.cpp. Strips:
 *   - recompui (UI overlay; no menu yet)
 *   - mod loader / texture pack subsystem
 *   - Native file dialogs
 *   - Game-specific config + autosave
 *
 * Keeps:
 *   - SDL2 audio queue (queue_samples / get_frames_remaining /
 *     set_frequency / reset_audio)
 *   - SDL2 window + GameController input
 *   - RT64 render context creation via pokestadium::renderer
 *   - recomp::start with full callback wiring
 *
 * Per project principles: anywhere we'd want to "skip" something
 * with a stub, we either implement it for real or use a
 * loud-abort pattern that surfaces missing functionality.
 */

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// Avoid Windows.h macro pollution clobbering identifiers in
// librecomp/ultramodern headers below.
#undef ERROR
#undef OUT
#undef IN
#undef OPTIONAL
#undef min
#undef max

#include <SDL.h>
#include <SDL_syswm.h>

#include "recomp.h"
#include <librecomp/game.hpp>
#include <ultramodern/ultramodern.hpp>
#include <ultramodern/error_handling.hpp>

#include "pokestadium_render.h"

extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);

// RSP microcode entry points provided by RSPRecomp output (Zelda's
// built aspMain.cpp + njpgdspMain.cpp at F:/Projects/Zelda64Recomp/rsp,
// referenced by CMakeLists). These implement the libultra standard
// audio/JPEG microcodes that ship with Pokemon Stadium-era N64 games.
extern RspUcodeFunc aspMain;
extern RspUcodeFunc njpgdspMain;

static RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
    switch (task->t.type) {
        case M_AUDTASK:   return aspMain;
        case M_NJPEGTASK: return njpgdspMain;
        default:
            fprintf(stderr, "[Pokemon Stadium] Unknown RSP task type: %u\n", task->t.type);
            return nullptr;
    }
}

// ---- Audio (ported from Zelda64Recomp main.cpp) ----------------------------
// Standard SDL2 audio queue with same channel-swap + resampling as
// Zelda64Recomp uses. Pokemon Stadium's audio output format matches
// MM/OoT (16-bit stereo from libultra audio thread).

constexpr int input_channels  = 2;
constexpr int output_channels = 2;
constexpr int bytes_per_frame = output_channels * sizeof(float);
// Frames duplicated at chunk boundaries to avoid resampling discontinuities.
constexpr size_t duplicated_input_frames = 4;

static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioCVT audio_convert{};
static uint32_t sample_rate        = 32000;
static uint32_t output_sample_rate = 48000;
static uint32_t discarded_output_frames = 0;

static void update_audio_converter() {
    int ret = SDL_BuildAudioCVT(&audio_convert, AUDIO_F32, input_channels, sample_rate,
                                AUDIO_F32, output_channels, output_sample_rate);
    if (ret < 0) {
        fprintf(stderr, "Error creating SDL audio converter: %s\n", SDL_GetError());
        throw std::runtime_error("Error creating SDL audio converter");
    }
    discarded_output_frames = static_cast<uint32_t>(duplicated_input_frames * output_sample_rate / sample_rate);
}

static void queue_samples(int16_t* audio_data, size_t sample_count) {
    static std::vector<float> swap_buffer;
    static std::array<float, duplicated_input_frames * input_channels> duplicated_sample_buffer{};

    size_t resampled_sample_count = sample_count + duplicated_input_frames * input_channels;
    size_t max_sample_count = std::max(resampled_sample_count, resampled_sample_count * size_t(audio_convert.len_mult));
    if (max_sample_count > swap_buffer.size()) {
        swap_buffer.resize(max_sample_count);
    }

    // Carry over duplicated frames from the previous chunk.
    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        swap_buffer[i] = duplicated_sample_buffer[i];
    }

    // Convert int16→float and swap stereo (libultra interleaves R,L per word).
    constexpr float main_volume = 1.0f;  // No volume control yet.
    for (size_t i = 0; i < sample_count; i += input_channels) {
        swap_buffer[i + 0 + duplicated_input_frames * input_channels] = audio_data[i + 1] * (0.5f / 32768.0f) * main_volume;
        swap_buffer[i + 1 + duplicated_input_frames * input_channels] = audio_data[i + 0] * (0.5f / 32768.0f) * main_volume;
    }

    if (sample_count <= duplicated_input_frames * input_channels) {
        // Chunk too small to reseed duplicated buffer; skip queueing this chunk.
        return;
    }
    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        duplicated_sample_buffer[i] = swap_buffer[i + sample_count];
    }

    audio_convert.buf = reinterpret_cast<Uint8*>(swap_buffer.data());
    audio_convert.len = static_cast<int>((sample_count + duplicated_input_frames * input_channels) * sizeof(swap_buffer[0]));

    if (SDL_ConvertAudio(&audio_convert) < 0) {
        fprintf(stderr, "Error using SDL audio converter: %s\n", SDL_GetError());
        return;
    }

    uint64_t cur_queued_microseconds = uint64_t(SDL_GetQueuedAudioSize(audio_device)) / bytes_per_frame * 1000000 / sample_rate;
    uint32_t num_bytes_to_queue = audio_convert.len_cvt - output_channels * discarded_output_frames * sizeof(swap_buffer[0]);
    float* samples_to_queue = swap_buffer.data() + output_channels * discarded_output_frames / 2;

    uint32_t skip_factor = static_cast<uint32_t>(cur_queued_microseconds / 100000);
    if (skip_factor != 0) {
        uint32_t skip_ratio = 1u << skip_factor;
        num_bytes_to_queue /= skip_ratio;
        for (size_t i = 0; i < num_bytes_to_queue / (output_channels * sizeof(swap_buffer[0])); i++) {
            samples_to_queue[2 * i + 0] = samples_to_queue[2 * skip_ratio * i + 0];
            samples_to_queue[2 * i + 1] = samples_to_queue[2 * skip_ratio * i + 1];
        }
    }

    SDL_QueueAudio(audio_device, samples_to_queue, num_bytes_to_queue);
}

static size_t get_frames_remaining() {
    constexpr float buffer_offset_frames = 1.0f;
    uint64_t buffered_byte_count = SDL_GetQueuedAudioSize(audio_device);
    buffered_byte_count = buffered_byte_count * 2 * sample_rate / output_sample_rate / output_channels;
    uint32_t frames_per_vi = (sample_rate / 60);
    if (buffered_byte_count > uint64_t(buffer_offset_frames * bytes_per_frame * frames_per_vi)) {
        buffered_byte_count -= uint64_t(buffer_offset_frames * bytes_per_frame * frames_per_vi);
    } else {
        buffered_byte_count = 0;
    }
    return static_cast<size_t>(buffered_byte_count / bytes_per_frame);
}

static void set_frequency(uint32_t freq) {
    sample_rate = freq;
    update_audio_converter();
}

static void reset_audio(uint32_t output_freq) {
    SDL_AudioSpec spec_desired{};
    spec_desired.freq    = (int)output_freq;
    spec_desired.format  = AUDIO_F32;
    spec_desired.channels = (Uint8)output_channels;
    spec_desired.samples = 0x100;

    audio_device = SDL_OpenAudioDevice(nullptr, false, &spec_desired, nullptr, 0);
    if (audio_device == 0) {
        fprintf(stderr, "SDL error opening audio device: %s\n", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }
    SDL_PauseAudioDevice(audio_device, 0);
    output_sample_rate = output_freq;
    update_audio_converter();
}

// ---- Window / gfx callbacks (SDL2-backed) ----------------------------------

static SDL_Window* g_window = nullptr;

static ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    return {};
}

static ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
    g_window = SDL_CreateWindow(
        "Pokemon Stadium (PokemonStadiumRecomp)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        960, 720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
    );
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }
    SDL_ShowWindow(g_window);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(g_window, &wmInfo);
#if defined(_WIN32)
    return ultramodern::renderer::WindowHandle{ wmInfo.info.win.window, GetCurrentThreadId() };
#elif defined(__linux__)
    return ultramodern::renderer::WindowHandle{ wmInfo.info.x11.window };
#elif defined(__APPLE__)
    return ultramodern::renderer::WindowHandle{ wmInfo.info.cocoa.window, nullptr };
#endif
}

static void update_gfx(void*) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            std::exit(0);
        }
    }
}

// ---- Input (minimal SDL GameController-only; no gyro / no remap UI) --------
// Per principles: no stub. If a real controller isn't connected, polling
// returns zeroed input — the game runs as if the player is idle, which is
// real behavior, not a fake. Real input takes effect when a controller is
// plugged in and SDL_GameControllerOpen succeeds.

static SDL_GameController* g_pad = nullptr;

static void poll_inputs() {
    SDL_GameControllerUpdate();
    if (!g_pad) {
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                g_pad = SDL_GameControllerOpen(i);
                if (g_pad) break;
            }
        }
    }
}

static bool get_n64_input(int controller_num, uint16_t* buttons_out, float* x_out, float* y_out) {
    *buttons_out = 0;
    *x_out = 0.0f;
    *y_out = 0.0f;

    if (controller_num != 0 || !g_pad) return false;

    auto pressed = [&](SDL_GameControllerButton b) {
        return SDL_GameControllerGetButton(g_pad, b) ? 1 : 0;
    };

    // N64 button bit layout (libultra contStat):
    //   0x8000 A         0x4000 B         0x2000 Z         0x1000 Start
    //   0x0800 D-Up      0x0400 D-Down    0x0200 D-Left    0x0100 D-Right
    //   0x0020 L         0x0010 R
    //   0x0008 C-Up      0x0004 C-Down    0x0002 C-Left    0x0001 C-Right
    uint16_t b = 0;
    if (pressed(SDL_CONTROLLER_BUTTON_A))             b |= 0x8000; // A
    if (pressed(SDL_CONTROLLER_BUTTON_B))             b |= 0x4000; // B
    if (pressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  b |= 0x2000; // Z
    if (pressed(SDL_CONTROLLER_BUTTON_START))         b |= 0x1000; // Start
    if (pressed(SDL_CONTROLLER_BUTTON_DPAD_UP))       b |= 0x0800;
    if (pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN))     b |= 0x0400;
    if (pressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT))     b |= 0x0200;
    if (pressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT))    b |= 0x0100;
    if (pressed(SDL_CONTROLLER_BUTTON_LEFTSTICK))     b |= 0x0020; // L
    if (pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) b |= 0x0010; // R
    *buttons_out = b;

    // C-buttons from right stick.
    int16_t rx = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTX);
    int16_t ry = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTY);
    constexpr int16_t deadzone = 8000;
    if (ry < -deadzone) *buttons_out |= 0x0008; // C-Up
    if (ry >  deadzone) *buttons_out |= 0x0004; // C-Down
    if (rx < -deadzone) *buttons_out |= 0x0002; // C-Left
    if (rx >  deadzone) *buttons_out |= 0x0001; // C-Right

    int16_t lx = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX);
    int16_t ly = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY);
    *x_out =  float(lx) / 32767.0f * 80.0f;  // N64 stick range ~[-80, 80]
    *y_out = -float(ly) / 32767.0f * 80.0f;
    return true;
}

static void set_rumble(int controller_num, bool on) {
    if (controller_num != 0 || !g_pad) return;
    SDL_GameControllerRumble(g_pad, on ? 0xFFFF : 0, on ? 0xFFFF : 0, 1000);
}

static ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num) {
    ultramodern::input::connected_device_info_t info{};
    info.connected_device = (controller_num == 0 && g_pad)
        ? ultramodern::input::Device::Controller
        : ultramodern::input::Device::None;
    info.connected_pak = ultramodern::input::Pak::None;
    return info;
}

// ---- Error handling -------------------------------------------------------

static void error_message_box(const char* msg) {
    // Always-on persistent error log — this fires before
    // ultramodern::error_handling::quick_exit() terminates the process,
    // and stderr buffering can swallow the message in headless runs.
    // Write a known file path so post-mortem inspection is easy.
    FILE* f = fopen("F:/Projects/PokemonStadiumRecomp/build/last_error.log", "a");
    if (f) {
        fprintf(f, "[PSR ERROR] %s\n", msg);
        fclose(f);
    }
    fprintf(stderr, "[PSR ERROR] %s\n", msg);
    fflush(stderr);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "PokemonStadiumRecomp", msg, nullptr);
}

// ---- Game thread name ----------------------------------------------------

static std::string get_game_thread_name(const OSThread* t) {
    return std::string("PokemonStadium");
}

// ---- Boot ------------------------------------------------------------------

// lookup.cpp defines this with C++ linkage (it's a .cpp file with no extern "C").
gpr get_entrypoint_address();

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    // Crash-localization breadcrumbs — flushed immediately so a silent
    // exit reveals exactly how far we got.
    std::fprintf(stderr, "[PSR] main() entered\n"); std::fflush(stderr);

#ifdef _WIN32
    // Force WASAPI for stable sample queueing.
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
#endif

    std::fprintf(stderr, "[PSR] before SDL_InitSubSystem\n"); std::fflush(stderr);

    SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);
    std::fprintf(stderr, "[PSR] SDL audio/controller init OK\n"); std::fflush(stderr);
    reset_audio(48000);
    std::fprintf(stderr, "[PSR] reset_audio done\n"); std::fflush(stderr);

    recomp::Version project_version{0, 1, 0, ""};
    recomp::register_config_path(std::filesystem::current_path());

    recomp::GameEntry game{};
    game.rom_hash               = 0xED1378BC12115F71ULL;  // first 8 bytes of MD5
    game.internal_name          = "POKEMON STADIUM";
    game.game_id                = u8"pokestadium.us.1.0";
    game.mod_game_id            = "pokestadium";
    game.save_type              = recomp::SaveType::Eep4k;  // Pokemon Stadium uses EEPROM
    game.is_enabled             = true;
    game.has_compressed_code    = false;
    game.entrypoint_address     = get_entrypoint_address();
    game.entrypoint             = recomp_entrypoint;
    recomp::register_game(game);
    std::fprintf(stderr, "[PSR] game registered, about to recomp::start\n"); std::fflush(stderr);

    recomp::rsp::callbacks_t rsp_callbacks{
        .get_rsp_microcode = get_rsp_microcode,
    };
    ultramodern::renderer::callbacks_t renderer_callbacks{
        .create_render_context = pokestadium::renderer::create_render_context,
    };
    ultramodern::audio_callbacks_t audio_callbacks{
        .queue_samples        = queue_samples,
        .get_frames_remaining = get_frames_remaining,
        .set_frequency        = set_frequency,
    };
    ultramodern::input::callbacks_t input_callbacks{
        .poll_input                 = poll_inputs,
        .get_input                  = get_n64_input,
        .set_rumble                 = set_rumble,
        .get_connected_device_info  = get_connected_device_info,
    };
    ultramodern::gfx_callbacks_t gfx_callbacks{
        .create_gfx    = create_gfx,
        .create_window = create_window,
        .update_gfx    = update_gfx,
    };
    // VI heartbeat — fires each VI interrupt. Logs to file every 60 ticks
    // (~1 second of game-time at 60Hz) so we can confirm the renderer is
    // alive without spamming stderr.
    static auto vi_heartbeat = []() {
        static int tick = 0;
        if ((tick++ % 60) == 0) {
            FILE* f = fopen("F:/Projects/PokemonStadiumRecomp/build/heartbeat.log", "a");
            if (f) {
                fprintf(f, "VI tick %d\n", tick);
                fclose(f);
            }
        }
    };
    ultramodern::events::callbacks_t events_callbacks{
        .vi_callback = vi_heartbeat,
    };
    ultramodern::error_handling::callbacks_t error_handling_callbacks{
        .message_box = error_message_box,
    };
    ultramodern::threads::callbacks_t threads_callbacks{
        .get_game_thread_name = get_game_thread_name,
    };

    recomp::start(
        project_version,
        {},  // window_handle (created by create_window callback)
        rsp_callbacks,
        renderer_callbacks,
        audio_callbacks,
        input_callbacks,
        gfx_callbacks,
        events_callbacks,
        error_handling_callbacks,
        threads_callbacks
    );

    std::fprintf(stderr, "[PSR] recomp::start returned\n"); std::fflush(stderr);
    return EXIT_SUCCESS;
}
