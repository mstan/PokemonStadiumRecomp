// recompui_launcher.cpp — PSR host glue for the shared recomp-ui pre-boot
// launcher (Dear ImGui). See include/recompui_launcher.h.
//
// This replaces the old RmlUi-over-RT64 overlay (src/main/ui_seam.cpp). Instead
// of drawing an overlay on the running game's render thread and holding the boot
// on a "PLAY" click, we run the console-generic recomp-ui launcher in its own
// SDL/GL window BEFORE the game's RT64 context exists, commit its settings to
// launcher.cfg, and then boot straight into the game.
//
// The launcher owns its own SDL session (launcher_platform_sdl2.c calls SDL_Init
// on open and SDL_Quit on close), so the caller (main.cpp) re-establishes the
// SDL subsystems + controller mappings the game needs after run() returns.

#define NOMINMAX

#include "recompui_launcher.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include <SDL.h>

#include "recomp_launcher.h"   // recomp-ui C ABI (recomp_launcher_run_window)
#include "launcher_profile.h"  // launcher_profile_apply("n64", &gi)

#include "app_paths.h"         // pkmnstadium::exe_dir()

namespace pkmnstadium::recompui {
namespace {

// Committed settings from the last run(); the source of truth for
// port_assignment(). The startup_* getters read launcher.cfg directly (with env
// overrides) so they match the pre-migration ui_seam behavior exactly.
RecompLauncherCSettings g_settings;
bool g_have_settings = false;

// ---- small cfg helpers (ported from ui_seam.cpp) --------------------------

std::string trim(std::string s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string to_lower_str(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

// Parse a cfg/env boolean. Accepts on/off, true/false, yes/no, 1/0 (any case).
bool parse_bool(const std::string& v, bool fallback) {
    const std::string s = to_lower_str(v);
    if (s == "1" || s == "on" || s == "true" || s == "yes") return true;
    if (s == "0" || s == "off" || s == "false" || s == "no") return false;
    return fallback;
}

// Read <exe>/launcher.cfg into a map. Keys are lowercased; values are trimmed
// but case-preserved (audio_device names are case-sensitive).
std::map<std::string, std::string> read_cfg_map() {
    std::map<std::string, std::string> m;
    std::ifstream f(pkmnstadium::exe_dir() / "launcher.cfg");
    std::string line;
    while (std::getline(f, line)) {
        const size_t c = line.find_first_of("#;");
        if (c != std::string::npos) line.resize(c);
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = to_lower_str(trim(line.substr(0, eq)));
        std::string v = trim(line.substr(eq + 1));
        if (!k.empty()) m[k] = v;
    }
    return m;
}

// ---- Transfer Pak inspector (ported faithfully from ui_seam.cpp) ----------

// Decode one char of the Gen-1 in-game text encoding.
char gen1_char(uint8_t b) {
    if (b >= 0x80 && b <= 0x99) return static_cast<char>('A' + (b - 0x80));
    if (b >= 0xA0 && b <= 0xB9) return static_cast<char>('a' + (b - 0xA0));
    if (b >= 0xF6 && b <= 0xFF) return static_cast<char>('0' + (b - 0xF6));
    if (b == 0x7F) return ' ';
    return '\0'; // unknown -> drop
}

// GameInfo.tpak_inspect callback: the HOST's cartridge brain. Sniffs the GB
// header @0x134 for the Gen-1 game key and decodes the Gen-1 save's trainer
// name + ID (gated on the main-data checksum). Matches the RecompLauncherCTpak
// contract in recomp_launcher.h (cart_kind: 1=red 2=blue 3=yellow 4=green).
int tpak_inspect(const char* rom_path, const char* save_path, RecompLauncherCTpak* out) {
    std::memset(out, 0, sizeof(*out));
    if (rom_path == nullptr || rom_path[0] == '\0') return 0;

    std::ifstream rf(rom_path, std::ios::binary);
    if (!rf) return 0;
    const std::vector<uint8_t> rom((std::istreambuf_iterator<char>(rf)),
                                   std::istreambuf_iterator<char>());
    if (rom.size() < 0x150) return 0; // not a readable GB rom

    // GB game key from the 16-byte title @ 0x134.
    const std::string title(reinterpret_cast<const char*>(rom.data() + 0x134), 16);
    auto has = [&](const char* s) { return title.find(s) != std::string::npos; };
    std::string key;
    if      (has("YELLOW")) key = "yellow";
    else if (has("RED"))    key = "red";
    else if (has("BLUE"))   key = "blue";
    else if (has("GREEN"))  key = "green";

    out->valid = 1;
    if      (key == "red")    { out->cart_kind = 1; std::snprintf(out->cart_label, sizeof(out->cart_label), "Pok\xC3\xA9mon Red"); }
    else if (key == "blue")   { out->cart_kind = 2; std::snprintf(out->cart_label, sizeof(out->cart_label), "Pok\xC3\xA9mon Blue"); }
    else if (key == "yellow") { out->cart_kind = 3; std::snprintf(out->cart_label, sizeof(out->cart_label), "Pok\xC3\xA9mon Yellow"); }
    else if (key == "green")  { out->cart_kind = 4; std::snprintf(out->cart_label, sizeof(out->cart_label), "Pok\xC3\xA9mon Green"); }
    else                      { out->cart_kind = 0; out->cart_label[0] = '\0'; }

    // Gen-1 save: trainer name + ID, only when the main-data checksum matches.
    if (save_path != nullptr && save_path[0] != '\0') {
        std::ifstream sf(save_path, std::ios::binary);
        if (sf) {
            const std::vector<uint8_t> sav((std::istreambuf_iterator<char>(sf)),
                                           std::istreambuf_iterator<char>());
            if (sav.size() >= 0x3524) {
                uint32_t sum = 0;
                for (size_t i = 0x2598; i <= 0x3522; ++i) sum += sav[i];
                const uint8_t calc = static_cast<uint8_t>(~(sum & 0xFF));
                if (calc == sav[0x3523]) {
                    // Trainer name: up to 7 chars, 0x50/0x00-terminated, @0x2598.
                    std::string name;
                    for (size_t i = 0x2598; i <= 0x25A2; ++i) {
                        const uint8_t c = sav[i];
                        if (c == 0x50 || c == 0x00) break;
                        const char ch = gen1_char(c);
                        if (ch != '\0') name.push_back(ch);
                    }
                    std::snprintf(out->trainer_name, sizeof(out->trainer_name), "%s", name.c_str());
                    // Trainer ID: 16-bit big-endian @0x2605, shown as 5 digits.
                    const int id = (sav[0x2605] << 8) | sav[0x2606];
                    std::snprintf(out->trainer_id, sizeof(out->trainer_id), "%05d", id);
                }
            }
        }
    }
    return out->valid;
}

// ---- launcher.cfg <-> RecompLauncherCSettings -----------------------------

void set_str(char* dst, size_t cap, const std::string& src) {
    std::snprintf(dst, cap, "%s", src.c_str());
}

// Seed a RecompLauncherCSettings from launcher.cfg (the persistent GUI store).
// Absent keys keep the built-in defaults, so a fresh install still launches.
void load_cfg_into_settings(RecompLauncherCSettings& s) {
    std::memset(&s, 0, sizeof(s));
    // Baseline defaults (also used when launcher.cfg is absent). renderer,
    // supersampling and antialiasing mirror PSR's historical launch defaults.
    s.enable_audio  = 1;
    s.audio_freq    = 48000;
    s.volume        = 100;
    s.output_method = 2; // OpenGL (unused by the RT64 game host; harmless)
    s.window_scale  = 3;
    s.renderer      = 0; // Auto
    s.supersampling = 2; // 2x
    s.antialiasing  = 4; // 4x MSAA
    s.fullscreen    = 0; // windowed

    const std::filesystem::path cfg_path = pkmnstadium::exe_dir() / "launcher.cfg";
    if (!std::filesystem::exists(cfg_path)) {
        // First run: a keyboard-driven Player 1 so the launcher (and the game,
        // if the launcher is unavailable) is usable out of the box. The user can
        // change the source in the launcher.
        s.player_src[0] = 1; // Keyboard
        return;
    }

    auto cfg = read_cfg_map();
    auto get = [&](const std::string& k) -> std::string {
        auto it = cfg.find(k);
        return it == cfg.end() ? std::string() : it->second;
    };

    for (int i = 0; i < 4; ++i) {
        const std::string p = "p" + std::to_string(i + 1);
        // Input source: pN_device 0=None, 1=Keyboard, 2+=gamepad -> player_src.
        // (Old configs stored a gamepad enumeration index in 2+; we collapse any
        // >=2 to "gamepad" — the specific pad is re-derived by enumeration order,
        // see port_assignment.)
        if (cfg.count(p + "_device")) {
            const int dev = std::atoi(cfg[p + "_device"].c_str());
            s.player_src[i] = (dev <= 0) ? 0 : (dev == 1 ? 1 : 2);
        }
        // Transfer Pak cart: an enabled pak stores pN_rom/pN_save (read by
        // transfer_pak.cpp); a pak toggled off stores pN_rom_off/pN_save_off so
        // the path is preserved without transfer_pak loading it.
        const std::string rom = get(p + "_rom");
        if (!rom.empty()) {
            set_str(s.tpak_rom_path[i], sizeof(s.tpak_rom_path[i]), rom);
            set_str(s.tpak_save_path[i], sizeof(s.tpak_save_path[i]), get(p + "_save"));
            s.tpak_enabled[i] = 1;
        } else {
            const std::string roff = get(p + "_rom_off");
            if (!roff.empty()) {
                set_str(s.tpak_rom_path[i], sizeof(s.tpak_rom_path[i]), roff);
                set_str(s.tpak_save_path[i], sizeof(s.tpak_save_path[i]), get(p + "_save_off"));
                s.tpak_enabled[i] = -1; // present but toggled off
            } else {
                s.tpak_enabled[i] = 0;
            }
        }
    }

    if (cfg.count("graphics_api")) {
        const std::string v = to_lower_str(cfg["graphics_api"]);
        s.renderer = (v == "vulkan") ? 1 : (v == "d3d12") ? 2 : 0;
    }
    if (cfg.count("supersampling")) {
        const std::string v = to_lower_str(cfg["supersampling"]);
        s.supersampling = (v == "off") ? 1 : (v == "4x") ? 4 : 2;
    }
    if (cfg.count("antialiasing")) {
        const std::string v = to_lower_str(cfg["antialiasing"]);
        s.antialiasing = (v == "off") ? 0 : (v == "2x") ? 2 : (v == "8x") ? 8 : 4;
    }
    if (cfg.count("window_mode")) {
        s.fullscreen = (to_lower_str(cfg["window_mode"]) == "fullscreen") ? 1 : 0;
    } else if (cfg.count("fullscreen")) {
        s.fullscreen = parse_bool(cfg["fullscreen"], false) ? 1 : 0;
    }
    if (cfg.count("audio_device")) {
        set_str(s.audio_device, sizeof(s.audio_device), cfg["audio_device"]);
    }
    if (cfg.count("autoplay")) {
        s.skip_launcher = parse_bool(cfg["autoplay"], false) ? 1 : 0;
    }
}

// Persist a committed RecompLauncherCSettings back to launcher.cfg, using the
// exact keys the readers expect: transfer_pak.cpp reads pN_rom/pN_save, and the
// startup_* getters read graphics_api/supersampling/antialiasing/window_mode/
// audio_device.
void write_settings_to_cfg(const RecompLauncherCSettings& s) {
    std::ofstream f(pkmnstadium::exe_dir() / "launcher.cfg", std::ios::trunc);
    if (!f) {
        std::fprintf(stderr, "[recompui] WARN: cannot write launcher.cfg\n");
        return;
    }
    f << "# PokemonStadiumRecomp launcher configuration - managed by the launcher.\n";
    f << "# pN_device : input source for player N (0=None, 1=Keyboard, 2=Gamepad).\n";
    f << "# pN_enabled: whether player N is active (mirrors pN_device != None).\n";
    f << "# pN_rom / pN_save: the Transfer Pak GB cartridge + save for player N.\n";
    f << "#   A pak toggled off keeps its cart under pN_rom_off/pN_save_off so\n";
    f << "#   the emulated Transfer Pak (which reads pN_rom) does not load it.\n";
    for (int i = 0; i < 4; ++i) {
        const int n = i + 1;
        int src = s.player_src[i];
        if (src < 0) src = 0;
        if (src > 2) src = 2;
        f << "p" << n << "_enabled=" << (src != 0 ? "on" : "off") << "\n";
        f << "p" << n << "_device=" << src << "\n";
        const char* rom = s.tpak_rom_path[i];
        const char* sav = s.tpak_save_path[i];
        if (rom[0] != '\0') {
            // launcher_model_tpak_enabled(): >0 on, <0 off, 0 (unset) -> on when
            // a cart is present. So enabled iff tpak_enabled != -1.
            const bool on = s.tpak_enabled[i] >= 0;
            const char* rk = on ? "_rom"  : "_rom_off";
            const char* sk = on ? "_save" : "_save_off";
            f << "p" << n << rk << "=" << rom << "\n";
            if (sav[0] != '\0') f << "p" << n << sk << "=" << sav << "\n";
            f << "p" << n << "_tpak=" << (on ? "on" : "off") << "\n";
        }
    }
    const char* api = (s.renderer == 1) ? "vulkan" : (s.renderer == 2) ? "d3d12" : "auto";
    const char* ss  = (s.supersampling <= 1) ? "off" : (s.supersampling >= 4) ? "4x" : "2x";
    const char* aa  = (s.antialiasing == 0) ? "off"
                    : (s.antialiasing == 2) ? "2x"
                    : (s.antialiasing == 8) ? "8x" : "4x";
    f << "# graphics_api=auto|vulkan|d3d12 : render backend.\n";
    f << "graphics_api=" << api << "\n";
    f << "# supersampling=off|2x|4x : 3D supersampling depth.\n";
    f << "supersampling=" << ss << "\n";
    f << "# antialiasing=off|2x|4x|8x : MSAA sample count.\n";
    f << "antialiasing=" << aa << "\n";
    f << "# window_mode=windowed|fullscreen : how the game window opens.\n";
    f << "window_mode=" << (s.fullscreen ? "fullscreen" : "windowed") << "\n";
    f << "# audio_device : output device name substring (empty = system default).\n";
    f << "audio_device=" << s.audio_device << "\n";
    f << "# autoplay=on|off : skip the launcher next time (recomp-ui skip flag).\n";
    f << "autoplay=" << (s.skip_launcher ? "on" : "off") << "\n";
}

// ---- host enumeration for the launcher's dropdowns ------------------------

std::vector<std::string> enumerate_audio_devices() {
    std::vector<std::string> out;
    const int n = SDL_GetNumAudioDevices(0 /* output */);
    for (int i = 0; i < n; ++i) {
        const char* nm = SDL_GetAudioDeviceName(i, 0);
        if (nm != nullptr && nm[0] != '\0') out.emplace_back(nm);
    }
    return out;
}

// The instance id of the k-th connected SDL game controller (device-index
// order). recomp-ui's C ABI returns player_src (None/Keyboard/Gamepad) but NOT
// which specific pad each gamepad-player picked, so gamepad-players are assigned
// pads by port order here. Enumerated after main re-inits SDL post-launcher.
int nth_gamepad_instance(int k) {
    if (k < 0) return -1;
    SDL_GameControllerUpdate();
    int seen = 0;
    const int njoy = SDL_NumJoysticks();
    for (int i = 0; i < njoy; ++i) {
        if (!SDL_IsGameController(i)) continue;
        if (seen == k) return static_cast<int>(SDL_JoystickGetDeviceInstanceID(i));
        ++seen;
    }
    return -1;
}

} // namespace

// ---------------------------------------------------------------------------

bool run(const char* rom_path_in, char* out_rom, std::size_t out_len) {
    RecompLauncherCSettings settings;
    load_cfg_into_settings(settings);

    RecompLauncherCGameInfo gi;
    std::memset(&gi, 0, sizeof(gi));
    // System identity + RT64 capability defaults (has_renderer/supersampling/
    // antialiasing/fullscreen), then the per-game facts a Stadium host adds.
    launcher_profile_apply("n64", &gi);
    gi.name         = "Pok\xC3\xA9mon Stadium";
    gi.region       = "USA";   // target ROM is US v1.0
    // ROM identity: SHA-1 of the target US v1.0 dump (MD5 ed1378bc..., the pin
    // asserted in CLAUDE.md). The launcher hashes the picked ROM and shows
    // "verified" on a match, so its check agrees with the runtime's real gate.
    static const char* const kStadiumSha1[] = {
        "ed7bef5a306f88c0a6e96b15e71fee2ef32058f3",
    };
    gi.known_sha1_hex = kStadiumSha1;
    gi.num_known_sha1 = 1;
    gi.num_players  = 4;
    gi.tpak_slots   = 4;
    gi.tpak_inspect = &tpak_inspect;
    gi.hide_rebind  = 0;
    gi.sram_path    = nullptr; // Stadium manages its own FlashRAM save

    static const char* const kRenderers[3] = { "Auto", "Vulkan", "D3D12" };
    gi.renderer_labels = kRenderers;
    gi.num_renderers   = 3;

    // The rebind page edits PSR's own input.cfg (next to the exe) — the same
    // file pkmnstadium::input reads; recomp-ui's n64_binds.c replicates its
    // on-disk format byte-for-byte. N64 has no launcher-editable [KeyMap]
    // hotkeys, so config_path stays null.
    static std::string s_keybinds_path;
    s_keybinds_path  = (pkmnstadium::exe_dir() / "input.cfg").string();
    gi.keybinds_path = s_keybinds_path.c_str();
    gi.config_path   = nullptr;

    // Host-enumerated audio output devices for Settings > Audio (SDL audio is
    // initialized by main.cpp before run() is called).
    static std::vector<std::string> s_dev_names;
    static std::vector<const char*> s_dev_ptrs;
    s_dev_names = enumerate_audio_devices();
    s_dev_ptrs.clear();
    for (auto& d : s_dev_names) s_dev_ptrs.push_back(d.c_str());
    gi.audio_device_labels = s_dev_ptrs.empty() ? nullptr : s_dev_ptrs.data();
    gi.num_audio_devices   = static_cast<int>(s_dev_ptrs.size());

    // recomp-ui resolves its own assets next to the exe (SDL base path); this is
    // passed for completeness. recomp_ui.cmake stages assets/fonts + assets/img.
    static std::string s_assets;
    s_assets = (pkmnstadium::exe_dir() / "assets").string();

    char chosen[1024] = {0};
    const int rc = recomp_launcher_run_window(
        "Pok\xC3\xA9mon Stadium \xE2\x80\x94 Launcher", // em dash
        &settings, &gi, s_assets.c_str(),
        (rom_path_in != nullptr && rom_path_in[0] != '\0') ? rom_path_in : nullptr,
        chosen, sizeof(chosen));

    if (rc == 1) { // QUIT
        std::fprintf(stderr, "[recompui] launcher: QUIT\n");
        std::fflush(stderr);
        return false;
    }

    // rc == 0 LAUNCH: commit + persist. rc == 2 UNAVAILABLE: no window opened
    // (assets/GL failed) -> boot as if skipped, using the settings we loaded.
    g_settings = settings;
    g_have_settings = true;
    if (rc == 0) {
        write_settings_to_cfg(settings);
    }
    if (out_rom != nullptr && out_len != 0) {
        if (rc == 0 && chosen[0] != '\0')
            std::snprintf(out_rom, out_len, "%s", chosen);
        else if (rom_path_in != nullptr && rom_path_in[0] != '\0')
            std::snprintf(out_rom, out_len, "%s", rom_path_in);
        else
            out_rom[0] = '\0';
    }
    std::fprintf(stderr, "[recompui] launcher: %s -> booting\n",
                 rc == 0 ? "LAUNCH" : "UNAVAILABLE (skip)");
    std::fflush(stderr);
    return true;
}

PortAssignment port_assignment(int port) {
    PortAssignment pa;
    if (port < 0 || port > 3 || !g_have_settings) return pa;

    const int src = g_settings.player_src[port];
    const char* rom = g_settings.tpak_rom_path[port];
    const bool tpak_on = rom[0] != '\0' && g_settings.tpak_enabled[port] >= 0;

    // A port responds if it has an input device OR an enabled Transfer Pak cart
    // (a TP-only port answers reads with idle input; see get_n64_input).
    pa.enabled = (src != 0) || tpak_on;
    if (src == 1) {
        pa.kind = DeviceKind::Keyboard;
    } else if (src == 2) {
        pa.kind = DeviceKind::Gamepad;
        // Assign connected pads to gamepad-players in port order.
        int ordinal = 0;
        for (int j = 0; j < port; ++j)
            if (g_settings.player_src[j] == 2) ++ordinal;
        pa.gamepad_instance = nth_gamepad_instance(ordinal);
    }
    return pa;
}

// ---- launch-time settings getters (launcher.cfg + PSR_* env) --------------
// Ported from ui_seam.cpp so the resolution/precedence is byte-identical.

bool startup_fullscreen() {
    auto cfg = read_cfg_map();
    bool pref = false;
    if (cfg.count("window_mode")) pref = (to_lower_str(cfg["window_mode"]) == "fullscreen");
    else if (cfg.count("fullscreen")) pref = parse_bool(cfg["fullscreen"], false);

    if (const char* wm = std::getenv("PSR_WINDOW_MODE"); wm != nullptr && wm[0] != '\0') {
        const std::string s = to_lower_str(wm);
        if (s == "fullscreen") return true;
        if (s == "windowed")   return false;
    }
    if (const char* fs = std::getenv("PSR_FULLSCREEN"); fs != nullptr && fs[0] != '\0') {
        return parse_bool(fs, pref);
    }
    return pref;
}

std::string startup_graphics_api() {
    std::string val = "auto";
    auto cfg = read_cfg_map();
    if (cfg.count("graphics_api")) val = cfg["graphics_api"];
    if (const char* e = std::getenv("PSR_GRAPHICS_API"); e != nullptr && e[0] != '\0') val = e;
    val = to_lower_str(val);
    if (val == "vk") val = "vulkan";
    if (val == "dx12" || val == "directx") val = "d3d12";
    if (val != "vulkan" && val != "d3d12") val = "auto";
    return val;
}

int startup_ds_option() {
    std::string val = "2x";
    auto cfg = read_cfg_map();
    if (cfg.count("supersampling")) val = cfg["supersampling"];
    val = to_lower_str(val);
    if (val == "off") return 1;
    if (val == "4x")  return 4;
    return 2; // "2x" (default)
}

int startup_msaa() {
    std::string val = "4x";
    auto cfg = read_cfg_map();
    if (cfg.count("antialiasing")) val = cfg["antialiasing"];
    val = to_lower_str(val);
    if (val == "off" || val == "0" || val == "none") return 0;
    if (val == "2x"  || val == "2") return 2;
    if (val == "8x"  || val == "8") return 8;
    return 4; // default / "4x"
}

std::string startup_audio_device() {
    std::string val;
    auto cfg = read_cfg_map();
    if (cfg.count("audio_device")) val = cfg["audio_device"];
    if (const char* e = std::getenv("PSR_AUDIO_DEVICE"); e != nullptr && e[0] != '\0') val = e;
    return val;
}

} // namespace pkmnstadium::recompui
