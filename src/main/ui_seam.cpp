/*
 * ui_seam.cpp — SS Anne RmlUi <-> RT64 seam proof (Phase 0).
 *
 * Minimal always-on overlay that renders a trivial RmlUi document over the
 * running game through RT64's render hooks. Its only job is to prove that the
 * recompui rendering path (RmlUi driven by RT64::RmlRenderInterface_RT64)
 * works inside Pokemon Stadium Recomp BEFORE the full "SS Anne" launcher is
 * built on top of it (Phase 1).
 *
 * This deliberately bypasses recompui's ui_state.cpp menu machinery (which is
 * still Zelda64-specific: mod menu, config tabs, launcher contexts). We reuse
 * only the framework-level, game-agnostic pieces:
 *   - recompui::RmlRenderInterface_RT64  (src/ui/ui_renderer.cpp)
 *   - SystemInterface_SDL                (lib/RmlUi/Backends)
 *   - RmlUi core + the Interface{VS,PS} shaders
 *
 * Once this renders, Phase 1 replaces the trivial document with the real
 * launcher and (re)introduces ui_state's context system as needed.
 *
 * Copyright (c) 2026 Matthew Stanley
 */

// NOMINMAX before anything that pulls in Windows.h (RT64 headers do).
#define NOMINMAX

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL.h>
#include <concurrentqueue.h>
#include <nfd.h>

#include "RmlUi/Core.h"
#include "RmlUi_Platform_SDL.h"

#include "rt64_render_hooks.h"
#include "ui_seam.h"

// Windows' <combaseapi.h> (pulled in transitively above) defines `interface`
// as a macro for `struct`, which collides with the `interface` parameter name
// in ui_renderer.h's RmlRenderInterface_RT64::init(). Drop the macro before
// including it.
#ifdef interface
#undef interface
#endif
#include "ui_renderer.h" // recompui::RmlRenderInterface_RT64

#ifdef _WIN32
#include <windows.h>
#endif

// Defined in main.cpp, assigned in create_window(). recompui's ui_state.cpp
// expects this exact `window` global as well, so defining it now keeps Phase 1
// drop-in.
extern SDL_Window* window;

namespace {

struct SeamUI {
    std::unique_ptr<SystemInterface_SDL> system_interface;
    recompui::RmlRenderInterface_RT64 render_interface;
    Rml::Context* context = nullptr;
    int width = 0;
    int height = 0;
};

std::unique_ptr<SeamUI> g_ui;

// ---- interactivity state (Phase 3) ----------------------------------------
// The launcher is the first screen: it captures input and renders over a blank
// frame until the user clicks PLAY, at which point it hides and main.cpp boots
// the game. Input is pumped on the gfx thread (main.cpp update_gfx) and drained
// on the RT64 render thread (draw_hook), so events cross threads via this queue.
std::atomic<bool> g_launcher_visible{true};
std::atomic<bool> g_play_requested{false};
moodycamel::ConcurrentQueue<SDL_Event> g_event_queue;

// An Rml::EventListener that runs a std::function. Instances are owned by
// g_listeners for the lifetime of the document.
class FnListener : public Rml::EventListener {
public:
    explicit FnListener(std::function<void()> fn) : fn_(std::move(fn)) {}
    void ProcessEvent(Rml::Event&) override {
        if (fn_) fn_();
    }
private:
    std::function<void()> fn_;
};
std::vector<std::unique_ptr<FnListener>> g_listeners;

// ---- controller assignment + enabled state (Phase 3b/3c) ------------------
// Device index space: 0 = None, 1 = Keyboard, 2.. = g_gamepads[idx-2].
std::vector<std::pair<int, std::string>> g_gamepads; // (SDL_JoystickID, name)
bool g_slot_enabled[4] = {false, false, false, false};
bool g_slot_has_rom[4] = {false, false, false, false};
int g_slot_device[4] = {0, 0, 0, 0}; // device index per slot

// ---- per-slot ROM/save, persisted to transfer_pak.cfg (Phase 3d) ----------
// transfer_pak.cfg is the GUI's persistent storage: read on load, rewritten on
// every Change. Mutated by the file-dialog thread, consumed by the render
// thread on the dirty flag, so guarded by g_change_mutex.
std::mutex g_change_mutex;
std::string g_slot_rom[4];   // pN_rom (as written in the cfg)
std::string g_slot_save[4];  // pN_save
bool g_slot_dirty[4] = {false, false, false, false};
std::atomic<bool> g_dialog_open{false}; // one native dialog at a time
Rml::ElementDocument* g_doc = nullptr;   // the loaded launcher document
std::string g_stadium_rom;               // newly-picked N64 ROM (pending)
bool g_stadium_dirty = false;
// A verified Stadium ROM is loaded whenever the launcher runs (startup verifies
// it), and the Stadium picker rejects invalid files, so this stays true. PLAY
// requires it (no valid Stadium ROM -> grayed).
bool g_stadium_valid = true;
// Transient warning banner (set from the dialog thread, shown on the render thread).
std::string g_banner_msg;
bool g_banner_dirty = false;
int g_banner_frames = 0; // render-thread countdown; >0 means visible

// Auto-play: a 5s countdown (default on) that launches once the config is valid,
// so a controller-only user can just wait. Render-thread state.
bool g_autoplay = true;
bool g_autoplay_armed = false;
std::chrono::steady_clock::time_point g_autoplay_deadline;
int g_autoplay_last_sec = -1;
int g_launcher_frames = 0; // frames the launcher has actually rendered

// Controller/keyboard navigation: a focus ring over the navigable items, with a
// sub-mode for an open controller dropdown. A=activate, B=back, stick/dpad/arrows
// move. Mouse still works independently.
std::vector<std::string> g_nav_items;                 // ordered focusable ids
std::map<std::string, std::function<void()>> g_nav_actions; // activate handler per id
int g_nav_index = 0;
int g_nav_menu_slot = -1;   // >=0: navigating that slot's open dropdown
int g_nav_menu_index = 0;
bool g_nav_axis_x = false;   // stick debounce
bool g_nav_axis_y = false;

int device_count() { return 2 + static_cast<int>(g_gamepads.size()); }

std::string device_name(int idx) {
    if (idx <= 0) return "None";
    if (idx == 1) return "Keyboard";
    const int gi = idx - 2;
    if (gi >= 0 && gi < static_cast<int>(g_gamepads.size())) return g_gamepads[gi].second;
    return "None";
}

// Advance to the next device index not already claimed by another slot
// (None is always selectable). Enforces "1 controller per slot".
int next_device(int slot, int cur) {
    const int n = device_count();
    for (int step = 1; step <= n; ++step) {
        const int cand = (cur + step) % n;
        if (cand == 0) return 0; // None
        bool used = false;
        for (int j = 0; j < 4; ++j) {
            if (j != slot && g_slot_device[j] == cand) { used = true; break; }
        }
        if (!used) return cand;
    }
    return 0;
}

bool config_valid() {
    if (!g_stadium_valid) return false; // a valid Stadium ROM is required
    for (int i = 0; i < 4; ++i) {
        if (g_slot_enabled[i] && g_slot_device[i] != 0) return true;
    }
    return false; // a controller is required to play
}

// A valid Game Boy ROM: header checksum at 0x14D over bytes 0x134..0x14C.
bool is_valid_gb_rom(const std::filesystem::path& rom) {
    std::ifstream f(rom, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (b.size() < 0x150) return false;
    uint8_t x = 0;
    for (int i = 0x134; i <= 0x14C; ++i) x = static_cast<uint8_t>(x - b[i] - 1);
    return x == b[0x14D];
}

// A valid Pokemon Stadium ROM: big-endian .z64 magic + "POKEMON STADIUM" in the
// internal name at 0x20.
bool is_valid_stadium_rom(const std::filesystem::path& rom) {
    std::ifstream f(rom, std::ios::binary);
    if (!f) return false;
    uint8_t h[0x34] = {0};
    f.read(reinterpret_cast<char*>(h), sizeof(h));
    if (f.gcount() < static_cast<std::streamsize>(sizeof(h))) return false;
    if (!(h[0] == 0x80 && h[1] == 0x37 && h[2] == 0x12 && h[3] == 0x40)) return false;
    const std::string name(reinterpret_cast<char*>(h + 0x20), 20);
    return name.find("POKEMON STADIUM") != std::string::npos;
}

// Queue a transient warning banner (safe from the dialog thread).
void set_banner(const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_change_mutex);
    g_banner_msg = msg;
    g_banner_dirty = true;
}

std::filesystem::path exe_dir() {
#ifdef _WIN32
    char buf[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

// ---- transfer_pak.cfg -> launcher slot population --------------------------

std::string trim(std::string s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Identify the Gen-1 game from the GB ROM header title (16 bytes @ 0x134).
// Returns a lowercase key ("yellow"/"red"/"blue"/"green") or "" if unknown.
std::string gb_game_key(const std::filesystem::path& rom) {
    std::ifstream f(rom, std::ios::binary);
    if (!f) return {};
    f.seekg(0x134);
    char buf[16] = {0};
    f.read(buf, sizeof(buf));
    std::string title(buf, sizeof(buf));
    auto has = [&](const char* s) { return title.find(s) != std::string::npos; };
    if (has("YELLOW")) return "yellow";
    if (has("RED")) return "red";
    if (has("BLUE")) return "blue";
    if (has("GREEN")) return "green";
    return {};
}

// ---- Gen-1 (.sav) trainer name + ID --------------------------------------
//
// Red/Blue/Yellow share an identical SRAM layout. The "main data" block runs
// 0x2598..0x3522 with a 1-byte complement checksum at 0x3523. There is NO
// game-version field, so a save cannot tell Red from Blue — we can only
// confirm it's a structurally valid Gen-1 save (which game is shown is driven
// by the cartridge header, per gb_game_key).
struct SaveInfo {
    bool present = false;
    bool valid = false; // Gen-1 main-data checksum matches
    std::string trainer; // decoded player name
    int id = -1;         // 16-bit trainer ID
};

// Decode one char of the Gen-1 in-game text encoding.
char gen1_char(uint8_t b) {
    if (b >= 0x80 && b <= 0x99) return static_cast<char>('A' + (b - 0x80));
    if (b >= 0xA0 && b <= 0xB9) return static_cast<char>('a' + (b - 0xA0));
    if (b >= 0xF6 && b <= 0xFF) return static_cast<char>('0' + (b - 0xF6));
    if (b == 0x7F) return ' ';
    return '\0'; // unknown -> drop
}

SaveInfo read_gen1_save(const std::filesystem::path& save_path) {
    SaveInfo out;
    std::ifstream f(save_path, std::ios::binary);
    if (!f) return out;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    out.present = true;
    if (b.size() < 0x3524) return out; // too small to be a Gen-1 SRAM image

    // Player name: up to 7 chars, 0x50-terminated, at 0x2598.
    for (size_t i = 0x2598; i <= 0x25A2; ++i) {
        const uint8_t c = b[i];
        if (c == 0x50 || c == 0x00) break;
        const char ch = gen1_char(c);
        if (ch != '\0') out.trainer.push_back(ch);
    }
    // Trainer ID: 16-bit big-endian at 0x2605.
    out.id = (b[0x2605] << 8) | b[0x2606];

    // Main-data checksum at 0x3523 = complement of the low byte of the sum of
    // 0x2598..0x3522.
    uint32_t sum = 0;
    for (size_t i = 0x2598; i <= 0x3522; ++i) sum += b[i];
    const uint8_t calc = static_cast<uint8_t>(~(sum & 0xFF));
    out.valid = (calc == b[0x3523]);
    return out;
}

void set_text(Rml::ElementDocument* doc, const std::string& id, const std::string& txt) {
    if (Rml::Element* e = doc->GetElementById(id)) {
        e->SetInnerRML(txt);
    }
}

void set_cart(Rml::ElementDocument* doc, const std::string& id, const std::string& src) {
    if (Rml::Element* e = doc->GetElementById(id)) {
        e->SetAttribute("src", src);
    }
}

// recompui's RmlRenderInterface_RT64::LoadTexture resolves <img src> against a
// pre-registered byte map (NOT the disk), so every cart image must be queued
// here under the exact key RmlUi passes to LoadTexture. We register under both
// the relative src ("carts/x.png") and the resolved absolute path so it matches
// regardless of RmlUi's path resolution.
void queue_cart_images(const std::filesystem::path& assets) {
    const char* names[] = {"yellow", "red", "blue", "empty", "n64", "check", "chev"};
    for (const char* n : names) {
        const std::filesystem::path p = assets / "carts" / (std::string(n) + ".png");
        std::ifstream f(p, std::ios::binary);
        if (!f) {
            std::fprintf(stderr, "[ss-anne] WARN: cart image missing: %s\n", p.string().c_str());
            continue;
        }
        const std::vector<char> bytes((std::istreambuf_iterator<char>(f)),
                                      std::istreambuf_iterator<char>());
        const std::string rel = "carts/" + std::string(n) + ".png";
        g_ui->render_interface.queue_image_from_bytes_file(rel, bytes);
        g_ui->render_interface.queue_image_from_bytes_file(p.string(), bytes);
        // RmlUi may normalize backslashes to forward slashes on the resolved path.
        std::string fwd = p.string();
        for (char& c : fwd) { if (c == '\\') c = '/'; }
        if (fwd != p.string()) {
            g_ui->render_interface.queue_image_from_bytes_file(fwd, bytes);
        }
    }
}

struct GameInfo { const char* display; const char* art; };
GameInfo info_for(const std::string& key) {
    if (key == "yellow") return {"Pok\xC3\xA9mon Yellow", "carts/yellow.png"};
    if (key == "red")    return {"Pok\xC3\xA9mon Red", "carts/red.png"};
    if (key == "blue")   return {"Pok\xC3\xA9mon Blue", "carts/blue.png"};
    if (key == "green")  return {"Pok\xC3\xA9mon Green", "carts/blue.png"};
    return {nullptr, nullptr};
}

void set_text(Rml::ElementDocument* doc, const std::string& id, const std::string& txt);
void set_cart(Rml::ElementDocument* doc, const std::string& id, const std::string& src);
void set_class(Rml::ElementDocument* doc, const std::string& id, const char* cls, bool on);

// Update one player card's ROM/save-derived fields (game name, cart art, save
// filename, trainer name + ID). Reused by the initial load and by live Change
// refreshes. RmlUi calls must run on the render thread.
void refresh_slot(Rml::ElementDocument* doc, int slot, const std::string& rom, const std::string& sav) {
    const std::string base = "card" + std::to_string(slot + 1);
    g_slot_has_rom[slot] = !rom.empty();

    // ROM controls: "Load ROM" when empty, "Change ROM" + a remove (x) when set.
    set_text(doc, base + "-romchange", rom.empty() ? "Load ROM" : "Change ROM");
    set_class(doc, base + "-romremove", "btn--collapsed", rom.empty());

    set_text(doc, base + "-trainer", "&#8212;");
    set_text(doc, base + "-id", "&#8212;");

    if (rom.empty()) {
        set_text(doc, base + "-game", "No game loaded");
        set_class(doc, base + "-game", "card__gamename--off", true);
        set_cart(doc, base + "-cart", "carts/empty.png");
        set_text(doc, base + "-save", "&#8212;");
        return;
    }
    set_class(doc, base + "-game", "card__gamename--off", false);

    std::filesystem::path rom_path = rom;
    if (rom_path.is_relative()) rom_path = std::filesystem::current_path() / rom_path;
    const GameInfo gi = info_for(gb_game_key(rom_path));
    if (gi.display != nullptr) {
        set_text(doc, base + "-game", gi.display);
        set_cart(doc, base + "-cart", gi.art);
    } else {
        set_text(doc, base + "-game", std::filesystem::path(rom).filename().string());
        set_cart(doc, base + "-cart", "carts/empty.png");
    }
    set_text(doc, base + "-save",
             sav.empty() ? "&#8212;" : std::filesystem::path(sav).filename().string());

    // Trainer name + ID come from the SAVE file's contents, not the cart.
    if (!sav.empty()) {
        std::filesystem::path sav_path = sav;
        if (sav_path.is_relative()) sav_path = std::filesystem::current_path() / sav_path;
        const SaveInfo si = read_gen1_save(sav_path);
        if (si.valid && !si.trainer.empty()) set_text(doc, base + "-trainer", si.trainer);
        if (si.valid && si.id >= 0) {
            char idbuf[8];
            std::snprintf(idbuf, sizeof(idbuf), "%05d", si.id); // Gen-1 shows 5-digit IDs
            set_text(doc, base + "-id", idbuf);
        }
        if (si.present && !si.valid) {
            std::fprintf(stderr, "[ss-anne] slot %d: save '%s' failed Gen-1 checksum\n",
                         slot + 1, sav_path.string().c_str());
        }
    }
}

// Rewrite transfer_pak.cfg from the current slot state. The cfg is the GUI's
// persistent storage. Caller must hold g_change_mutex.
void write_transfer_pak_cfg() {
    std::ofstream f(std::filesystem::current_path() / "transfer_pak.cfg", std::ios::trunc);
    if (!f) {
        std::fprintf(stderr, "[ss-anne] WARN: cannot write transfer_pak.cfg\n");
        return;
    }
    f << "# Transfer Pak carts - managed by the SS Anne launcher.\n";
    f << "# pN_rom / pN_save per player slot (1-4). Edited via the launcher's\n";
    f << "# cartridge (ROM) and Save 'Change' buttons.\n";
    for (int i = 0; i < 4; ++i) {
        if (!g_slot_rom[i].empty())  f << "p" << (i + 1) << "_rom=" << g_slot_rom[i] << "\n";
        if (!g_slot_save[i].empty()) f << "p" << (i + 1) << "_save=" << g_slot_save[i] << "\n";
    }
}

// Read transfer_pak.cfg into the slot state and paint every card.
void populate_slots_from_config(Rml::ElementDocument* doc, const std::filesystem::path& /*assets*/) {
    std::map<std::string, std::string> cfg;
    std::ifstream f(std::filesystem::current_path() / "transfer_pak.cfg");
    std::string line;
    while (std::getline(f, line)) {
        const size_t c = line.find_first_of("#;");
        if (c != std::string::npos) line.resize(c);
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = trim(line.substr(0, eq));
        std::string v = trim(line.substr(eq + 1));
        for (char& ch : k) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (!k.empty() && !v.empty()) cfg[k] = v;
    }

    for (int p = 1; p <= 4; ++p) {
        const std::string pk = "p" + std::to_string(p);
        g_slot_rom[p - 1]  = cfg.count(pk + "_rom")  ? cfg[pk + "_rom"]  : "";
        g_slot_save[p - 1] = cfg.count(pk + "_save") ? cfg[pk + "_save"] : "";
        // Slots with a configured cart default to enabled; empty slots disabled.
        g_slot_enabled[p - 1] = !g_slot_rom[p - 1].empty();
        refresh_slot(doc, p - 1, g_slot_rom[p - 1], g_slot_save[p - 1]);
    }
}

void add_click(Rml::ElementDocument* doc, const std::string& id, std::function<void()> fn) {
    Rml::Element* e = doc->GetElementById(id);
    if (e == nullptr) {
        std::fprintf(stderr, "[ss-anne] WARN: element '%s' not found for click handler\n", id.c_str());
        return;
    }
    auto listener = std::make_unique<FnListener>(std::move(fn));
    e->AddEventListener("click", listener.get());
    g_listeners.push_back(std::move(listener));
}

void set_class(Rml::ElementDocument* doc, const std::string& id, const char* cls, bool on) {
    if (Rml::Element* e = doc->GetElementById(id)) e->SetClass(cls, on);
}

// Refresh one card's controller name, status line, and enabled checkbox.
void update_slot_ui(Rml::ElementDocument* doc, int slot) {
    const std::string base = "card" + std::to_string(slot + 1);
    const bool assigned = g_slot_device[slot] != 0;
    set_text(doc, base + "-ctrl", device_name(g_slot_device[slot]));
    set_class(doc, base + "-chk", "chk--on", g_slot_enabled[slot]);
    if (Rml::Element* st = doc->GetElementById(base + "-status")) {
        // The label must be wrapped in a span: RmlUi drops a bare text node that
        // is a direct child of a flex row, leaving just the dot.
        if (g_slot_enabled[slot] && assigned) {
            st->SetInnerRML("<span class=\"dot dot--on\"></span><span class=\"st\">Connected</span>");
        } else {
            st->SetInnerRML("<span class=\"dot dot--off\"></span><span class=\"st\">Not assigned</span>");
        }
    }
}

// Enable PLAY only when the config is valid (a controller is required to play).
void update_play_gate(Rml::ElementDocument* doc) {
    set_class(doc, "play", "play--disabled", !config_valid());
}

// Stop the auto-play countdown and untick the box (called when the user starts
// configuring, so the game never launches out from under them).
void cancel_autoplay(Rml::ElementDocument* doc) {
    if (!g_autoplay) return;
    g_autoplay = false;
    g_autoplay_armed = false;
    set_class(doc, "autoplay-chk", "chk--on", false);
    set_class(doc, "autoplay-count", "hidden", true);
}

// Give the first enabled slot a sensible default device so PLAY can be reached
// quickly: the first gamepad if one is connected, otherwise the keyboard.
void apply_default_assignment() {
    for (int i = 0; i < 4; ++i) {
        if (g_slot_enabled[i]) {
            g_slot_device[i] = g_gamepads.empty() ? 1 /*Keyboard*/ : 2 /*first gamepad*/;
            break;
        }
    }
}

void hide_all_ctrl_menus(Rml::ElementDocument* doc) {
    for (int s = 0; s < 4; ++s) {
        set_class(doc, "card" + std::to_string(s + 1) + "-ctrlmenu", "hidden", true);
    }
}

// Assign device index `dev` to a slot, enforcing one device per slot (steal it
// from any other slot) and auto-enabling the slot when a real device is picked.
void select_device(Rml::ElementDocument* doc, int slot, int dev) {
    cancel_autoplay(doc);
    if (dev != 0) {
        for (int s = 0; s < 4; ++s) {
            if (s != slot && g_slot_device[s] == dev) {
                g_slot_device[s] = 0;
                update_slot_ui(doc, s);
            }
        }
        g_slot_enabled[slot] = true; // picking a controller activates the player
    }
    g_slot_device[slot] = dev;
    hide_all_ctrl_menus(doc);
    update_slot_ui(doc, slot);
    update_play_gate(doc);
}

// Open/close a slot's controller option list (closes the others).
void toggle_ctrl_menu(Rml::ElementDocument* doc, int slot) {
    cancel_autoplay(doc);
    const std::string id = "card" + std::to_string(slot + 1) + "-ctrlmenu";
    Rml::Element* menu = doc->GetElementById(id);
    if (menu == nullptr) return;
    const bool was_hidden = menu->IsClassSet("hidden");
    hide_all_ctrl_menus(doc);
    if (was_hidden) menu->SetClass("hidden", false);
}

// Populate a slot's controller menu with one row per device (None always first)
// and wire each row to select that device. Device list is static post-enumeration.
void build_ctrl_menu(Rml::ElementDocument* doc, int slot) {
    const std::string base = "card" + std::to_string(slot + 1);
    std::string rml;
    for (int d = 0; d < device_count(); ++d) {
        rml += "<div class=\"ctrl-opt\" id=\"" + base + "-opt-" + std::to_string(d) + "\">" +
               device_name(d) + "</div>";
    }
    if (Rml::Element* menu = doc->GetElementById(base + "-ctrlmenu")) {
        menu->SetInnerRML(rml);
    }
    for (int d = 0; d < device_count(); ++d) {
        add_click(doc, base + "-opt-" + std::to_string(d), [doc, slot, d] {
            select_device(doc, slot, d);
        });
    }
}

// ---- controller / keyboard navigation -------------------------------------

// Paint the nav-focus highlight on the current item (or open-menu option).
void apply_nav_highlight(Rml::ElementDocument* doc) {
    for (const auto& id : g_nav_items) set_class(doc, id, "nav-focus", false);
    for (int s = 0; s < 4; ++s) {
        for (int d = 0; d < device_count(); ++d) {
            set_class(doc, "card" + std::to_string(s + 1) + "-opt-" + std::to_string(d), "nav-focus", false);
        }
    }
    if (g_nav_menu_slot >= 0) {
        set_class(doc, "card" + std::to_string(g_nav_menu_slot + 1) + "-opt-" +
                       std::to_string(g_nav_menu_index), "nav-focus", true);
    } else if (!g_nav_items.empty()) {
        set_class(doc, g_nav_items[g_nav_index], "nav-focus", true);
    }
}

void nav_move(Rml::ElementDocument* doc, int delta) {
    if (g_nav_menu_slot >= 0) {
        const int n = device_count();
        if (n > 0) g_nav_menu_index = (g_nav_menu_index + delta + n) % n;
    } else if (!g_nav_items.empty()) {
        const int n = static_cast<int>(g_nav_items.size());
        g_nav_index = (g_nav_index + delta + n) % n;
    }
    apply_nav_highlight(doc);
}

void nav_back(Rml::ElementDocument* doc) {
    if (g_nav_menu_slot >= 0) {
        hide_all_ctrl_menus(doc);
        g_nav_menu_slot = -1;
        apply_nav_highlight(doc);
    }
}

void nav_activate(Rml::ElementDocument* doc) {
    if (g_nav_menu_slot >= 0) {
        select_device(doc, g_nav_menu_slot, g_nav_menu_index); // selects + hides menu
        g_nav_menu_slot = -1;
        apply_nav_highlight(doc);
        return;
    }
    if (g_nav_items.empty()) return;
    const std::string id = g_nav_items[g_nav_index];
    if (id.find("-dropdown") != std::string::npos) {
        const int slot = id[4] - '1'; // "cardN-dropdown"
        hide_all_ctrl_menus(doc);
        set_class(doc, "card" + std::to_string(slot + 1) + "-ctrlmenu", "hidden", false);
        g_nav_menu_slot = slot;
        g_nav_menu_index = (g_slot_device[slot] >= 0 && g_slot_device[slot] < device_count())
                               ? g_slot_device[slot] : 0;
        apply_nav_highlight(doc);
        return;
    }
    auto it = g_nav_actions.find(id);
    if (it != g_nav_actions.end() && it->second) it->second();
}

// Translate a controller/keyboard event into a nav action. Returns true if the
// event was consumed (so it isn't also handed to RmlUi).
bool handle_nav_event(Rml::ElementDocument* doc, const SDL_Event& ev) {
    switch (ev.type) {
    case SDL_CONTROLLERBUTTONDOWN:
        cancel_autoplay(doc);
        switch (ev.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_A: nav_activate(doc); break;
        case SDL_CONTROLLER_BUTTON_B: nav_back(doc); break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: nav_move(doc, -1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: nav_move(doc, +1); break;
        default: break;
        }
        return true;
    case SDL_CONTROLLERAXISMOTION: {
        const int v = ev.caxis.value;
        if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
            if (v < -16000 && !g_nav_axis_x) { g_nav_axis_x = true; cancel_autoplay(doc); nav_move(doc, -1); }
            else if (v > 16000 && !g_nav_axis_x) { g_nav_axis_x = true; cancel_autoplay(doc); nav_move(doc, +1); }
            else if (v > -8000 && v < 8000) { g_nav_axis_x = false; }
        } else if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            if (v < -16000 && !g_nav_axis_y) { g_nav_axis_y = true; cancel_autoplay(doc); nav_move(doc, -1); }
            else if (v > 16000 && !g_nav_axis_y) { g_nav_axis_y = true; cancel_autoplay(doc); nav_move(doc, +1); }
            else if (v > -8000 && v < 8000) { g_nav_axis_y = false; }
        }
        return true;
    }
    case SDL_KEYDOWN:
        switch (ev.key.keysym.sym) {
        case SDLK_LEFT: case SDLK_UP: cancel_autoplay(doc); nav_move(doc, -1); return true;
        case SDLK_RIGHT: case SDLK_DOWN: cancel_autoplay(doc); nav_move(doc, +1); return true;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: cancel_autoplay(doc); nav_activate(doc); return true;
        case SDLK_ESCAPE: case SDLK_BACKSPACE: nav_back(doc); return true;
        default: return false;
        }
    default:
        return false;
    }
}

// Run a native open-file dialog on a detached thread (so neither the render nor
// gfx thread blocks while it's modal), then persist the pick to transfer_pak.cfg
// and flag the card for refresh on the render thread.
enum class ChangeKind { Rom, Save };
void open_change_dialog(int slot, ChangeKind kind) {
    if (slot < 0 || slot > 3) return;
    if (g_dialog_open.exchange(true)) return; // one dialog at a time
    std::thread([slot, kind] {
        NFD_Init();
        nfdu8char_t* out = nullptr;
        nfdu8filteritem_t rom_filters[] = {{"Game Boy ROM", "gb,gbc,sgb"}};
        nfdu8filteritem_t save_filters[] = {{"Game Boy save", "sav,srm,sra"}};
        const nfdresult_t r = (kind == ChangeKind::Rom)
            ? NFD_OpenDialogU8(&out, rom_filters, 1, nullptr)
            : NFD_OpenDialogU8(&out, save_filters, 1, nullptr);
        if (r == NFD_OKAY && out != nullptr) {
            const std::string path(out);
            NFD_FreePathU8(out);
            const std::string fname = std::filesystem::path(path).filename().string();
            if (kind == ChangeKind::Rom) {
                if (!is_valid_gb_rom(path)) {
                    set_banner("Rejected \"" + fname + "\": not a valid Game Boy ROM");
                } else {
                    const bool recognized = !gb_game_key(path).empty();
                    {
                        std::lock_guard<std::mutex> lk(g_change_mutex);
                        const bool was_empty = g_slot_rom[slot].empty();
                        g_slot_rom[slot] = path;
                        if (was_empty) g_slot_enabled[slot] = true; // adding a cart enables it
                        g_slot_dirty[slot] = true;
                        write_transfer_pak_cfg();
                    }
                    if (!recognized) {
                        set_banner("Loaded \"" + fname + "\" — not a recognized Pokémon game");
                    }
                    std::fprintf(stderr, "[ss-anne] slot %d ROM -> %s\n", slot + 1, path.c_str());
                }
            } else {
                {
                    std::lock_guard<std::mutex> lk(g_change_mutex);
                    g_slot_save[slot] = path;
                    g_slot_dirty[slot] = true;
                    write_transfer_pak_cfg();
                }
                std::fprintf(stderr, "[ss-anne] slot %d save -> %s\n", slot + 1, path.c_str());
            }
        }
        NFD_Quit();
        g_dialog_open.store(false);
    }).detach();
}

// Remove the cart from a slot: clears ROM + save, disables it, persists.
void remove_cart(int slot) {
    if (slot < 0 || slot > 3) return;
    {
        std::lock_guard<std::mutex> lk(g_change_mutex);
        g_slot_rom[slot].clear();
        g_slot_save[slot].clear();
        g_slot_enabled[slot] = false;
        g_slot_device[slot] = 0; // free its controller
        g_slot_dirty[slot] = true;
        write_transfer_pak_cfg();
    }
    std::fprintf(stderr, "[ss-anne] slot %d cart removed\n", slot + 1);
}

// Pick a new Stadium (N64) ROM. Persisted to <exe>/rom.cfg, which main.cpp reads
// at startup — so it takes effect on the next launch (the ROM is already loaded
// this session).
void open_stadium_dialog() {
    if (g_dialog_open.exchange(true)) return;
    std::thread([] {
        NFD_Init();
        nfdu8char_t* out = nullptr;
        nfdu8filteritem_t filters[] = {{"N64 ROM", "z64,n64,v64"}};
        const nfdresult_t r = NFD_OpenDialogU8(&out, filters, 1, nullptr);
        if (r == NFD_OKAY && out != nullptr) {
            const std::string path(out);
            NFD_FreePathU8(out);
            const std::string fname = std::filesystem::path(path).filename().string();
            if (!is_valid_stadium_rom(path)) {
                set_banner("Rejected \"" + fname + "\": not a valid Pokémon Stadium (.z64) ROM");
            } else {
                std::ofstream cf(exe_dir() / "rom.cfg", std::ios::trunc);
                if (cf) cf << path << "\n";
                {
                    std::lock_guard<std::mutex> lk(g_change_mutex);
                    g_stadium_rom = path;
                    g_stadium_dirty = true;
                }
                set_banner("Stadium ROM set to \"" + fname + "\" — restart to apply");
                std::fprintf(stderr, "[ss-anne] stadium ROM -> %s (effective next launch)\n", path.c_str());
            }
        }
        NFD_Quit();
        g_dialog_open.store(false);
    }).detach();
}

// Wire up the launcher's interactive elements (Phase 3a/3b/3c/3d).
void attach_launcher_events(Rml::ElementDocument* doc) {
    apply_default_assignment();
    g_nav_items.clear();
    g_nav_actions.clear();

    for (int i = 0; i < 4; ++i) {
        const std::string base = "card" + std::to_string(i + 1);
        // Enabled toggle. (Any config interaction also cancels the auto-play
        // countdown so the game never launches mid-configuration.) Shared by the
        // mouse click and the controller/keyboard nav activate.
        auto enable_fn = [doc, i] {
            cancel_autoplay(doc);
            g_slot_enabled[i] = !g_slot_enabled[i];
            // Keep the controller assignment when disabling — harmless and config-friendly.
            update_slot_ui(doc, i);
            update_play_gate(doc);
        };
        add_click(doc, base + "-enable", enable_fn);
        g_nav_items.push_back(base + "-enable");
        g_nav_actions[base + "-enable"] = enable_fn;

        // Controller dropdown: mouse click toggles; nav activate opens + enters
        // menu sub-navigation (special-cased in nav_activate).
        build_ctrl_menu(doc, i);
        add_click(doc, base + "-dropdown", [doc, i] { toggle_ctrl_menu(doc, i); });
        g_nav_items.push_back(base + "-dropdown");

        // ROM/save controls (open native dialogs -> mouse/keyboard; not in the
        // controller ring).
        add_click(doc, base + "-romchange", [doc, i] { cancel_autoplay(doc); open_change_dialog(i, ChangeKind::Rom); });
        add_click(doc, base + "-romremove", [doc, i] { cancel_autoplay(doc); remove_cart(i); });
        add_click(doc, base + "-cart", [doc, i] { cancel_autoplay(doc); open_change_dialog(i, ChangeKind::Rom); });
        add_click(doc, base + "-savechange", [doc, i] { cancel_autoplay(doc); open_change_dialog(i, ChangeKind::Save); });
        update_slot_ui(doc, i);
    }

    // Stadium (N64) ROM picker.
    add_click(doc, "stadium-change", [doc] { cancel_autoplay(doc); open_stadium_dialog(); });

    // Auto-play toggle.
    auto autoplay_fn = [doc] {
        g_autoplay = !g_autoplay;
        set_class(doc, "autoplay-chk", "chk--on", g_autoplay);
        if (!g_autoplay) {
            g_autoplay_armed = false;
            set_class(doc, "autoplay-count", "hidden", true);
        }
    };
    add_click(doc, "autoplay-row", autoplay_fn);
    g_nav_items.push_back("autoplay-row");
    g_nav_actions["autoplay-row"] = autoplay_fn;

    // PLAY: only launches when the config is valid.
    auto play_fn = [doc] {
        if (!config_valid()) {
            std::fprintf(stderr, "[ss-anne] PLAY ignored: no enabled player has a controller\n");
            return;
        }
        if (g_play_requested.exchange(true)) return;
        g_launcher_visible.store(false);
        std::fprintf(stderr, "[ss-anne] PLAY -> launching game\n");
        std::fflush(stderr);
    };
    add_click(doc, "play", play_fn);
    g_nav_items.push_back("play");
    g_nav_actions["play"] = play_fn;

    update_play_gate(doc);
    // Start focused on PLAY so a controller user can press A immediately.
    g_nav_index = static_cast<int>(g_nav_items.size()) - 1;
    apply_nav_highlight(doc);
}

void init_hook(RT64::RenderInterface* render_interface, RT64::RenderDevice* device) {
#if defined(__linux__)
    std::locale::global(std::locale::classic());
#endif
    // PSR_AUTOBOOT bypasses the launcher entirely: don't build the overlay, mark
    // it not-visible so input routes straight to the game (main.cpp also boots
    // immediately in this mode). Prevents the launcher drawing over a running game.
    if (std::getenv("PSR_AUTOBOOT") != nullptr) {
        g_launcher_visible.store(false);
        std::fprintf(stderr, "[ss-anne] PSR_AUTOBOOT set -> launcher disabled\n");
        std::fflush(stderr);
        return;
    }
    g_ui = std::make_unique<SeamUI>();

    g_ui->system_interface = std::make_unique<SystemInterface_SDL>();
    g_ui->system_interface->SetWindow(window);
    g_ui->render_interface.init(render_interface, device);

    Rml::SetSystemInterface(g_ui->system_interface.get());
    Rml::SetRenderInterface(g_ui->render_interface.get_rml_interface());
    Rml::Initialise();

    const std::filesystem::path assets = exe_dir() / "assets";
    struct FontFace { const char* file; bool fallback; };
    const FontFace fonts[] = {
        {"LatoLatin-Regular.ttf", false},
        {"LatoLatin-Bold.ttf", false},
        {"NotoEmoji-Regular.ttf", true}, // fallback for check/controller/symbol glyphs
    };
    for (const FontFace& ff : fonts) {
        const std::filesystem::path fp = assets / ff.file;
        if (!Rml::LoadFontFace(fp.string(), ff.fallback)) {
            std::fprintf(stderr, "[ss-anne] WARN: failed to load font %s\n", fp.string().c_str());
        }
    }

    queue_cart_images(assets);

    SDL_GetWindowSizeInPixels(window, &g_ui->width, &g_ui->height);
    g_ui->context = Rml::CreateContext("ss_anne", Rml::Vector2i(g_ui->width, g_ui->height));
    if (g_ui->context != nullptr) {
        const std::filesystem::path doc_path = assets / "launcher.rml";
        Rml::ElementDocument* doc = g_ui->context->LoadDocument(doc_path.string());
        if (doc != nullptr) {
            g_doc = doc;
            populate_slots_from_config(doc, assets);
            attach_launcher_events(doc);
            doc->Show();
        } else {
            std::fprintf(stderr, "[ss-anne] ERROR: LoadDocument(%s) returned null\n",
                         doc_path.string().c_str());
        }
    }
    std::fprintf(stderr, "[ss-anne] launcher init: context=%p size=%dx%d\n",
                 (void*)g_ui->context, g_ui->width, g_ui->height);
    std::fflush(stderr);
}

void draw_hook(RT64::RenderCommandList* list, RT64::RenderFramebuffer* swap_chain_framebuffer) {
    if (!g_ui || g_ui->context == nullptr) {
        return;
    }
    // Once PLAY is clicked the launcher hides so the game's frames show through.
    if (!g_launcher_visible.load()) {
        return;
    }

    // Apply any pending Change (committed by the file-dialog thread) on this,
    // the render thread — RmlUi element mutation must not cross threads.
    {
        int slot = -1;
        std::string rom, sav, stadium, banner;
        bool stadium_dirty = false;
        bool banner_dirty = false;
        {
            std::lock_guard<std::mutex> lk(g_change_mutex);
            for (int i = 0; i < 4; ++i) {
                if (g_slot_dirty[i]) {
                    slot = i;
                    rom = g_slot_rom[i];
                    sav = g_slot_save[i];
                    g_slot_dirty[i] = false;
                    break;
                }
            }
            if (g_stadium_dirty) {
                stadium = g_stadium_rom;
                g_stadium_dirty = false;
                stadium_dirty = true;
            }
            if (g_banner_dirty) {
                banner = g_banner_msg;
                g_banner_dirty = false;
                banner_dirty = true;
            }
        }
        if (slot >= 0 && g_doc != nullptr) {
            refresh_slot(g_doc, slot, rom, sav);
            update_slot_ui(g_doc, slot);
            update_play_gate(g_doc);
        }
        if (stadium_dirty && g_doc != nullptr) {
            if (Rml::Element* e = g_doc->GetElementById("stadium__title")) {
                e->SetInnerRML(std::filesystem::path(stadium).filename().string() +
                               " <span id=\"stadium__verified\">(restart to apply)</span>");
            }
        }
        // Warning banner: show on new message, auto-hide after ~5s.
        if (g_doc != nullptr) {
            if (banner_dirty) {
                if (Rml::Element* e = g_doc->GetElementById("banner")) {
                    e->SetInnerRML(banner);
                    e->SetClass("hidden", false);
                }
                g_banner_frames = 300;
            } else if (g_banner_frames > 0) {
                if (--g_banner_frames == 0) {
                    if (Rml::Element* e = g_doc->GetElementById("banner")) {
                        e->SetClass("hidden", true);
                    }
                }
            }
        }
    }

    // Auto-play countdown: armed when autoplay is on, the config is valid, and no
    // file dialog is open. Counts down 5s and launches; cancelled by any config
    // interaction (cancel_autoplay) or by config becoming invalid.
    if (g_doc != nullptr) {
        // Don't start counting until the window has actually been on-screen for a
        // moment, so the visible countdown cleanly begins at 5 (not 4/3 after the
        // first-frame present lag).
        ++g_launcher_frames;
        const bool can = g_autoplay && config_valid() && !g_dialog_open.load() &&
                         g_launcher_frames > 30;
        if (can && !g_autoplay_armed) {
            g_autoplay_armed = true;
            g_autoplay_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            g_autoplay_last_sec = -1;
        } else if (!can && g_autoplay_armed) {
            g_autoplay_armed = false;
            if (Rml::Element* e = g_doc->GetElementById("autoplay-count")) e->SetClass("hidden", true);
        }
        if (g_autoplay_armed) {
            const auto rem_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                g_autoplay_deadline - std::chrono::steady_clock::now()).count();
            if (rem_ms <= 0) {
                g_autoplay_armed = false;
                if (!g_play_requested.exchange(true)) {
                    g_launcher_visible.store(false);
                    std::fprintf(stderr, "[ss-anne] auto-play countdown elapsed -> launching\n");
                    std::fflush(stderr);
                }
            } else {
                const int secs = static_cast<int>((rem_ms + 999) / 1000);
                if (secs != g_autoplay_last_sec) {
                    g_autoplay_last_sec = secs;
                    if (Rml::Element* e = g_doc->GetElementById("autoplay-count")) {
                        e->SetInnerRML("Auto-starting in " + std::to_string(secs) + "...");
                        e->SetClass("hidden", false);
                    }
                }
            }
        }
    }

    // Drain input events queued from the gfx thread. Controller/keyboard nav
    // events are consumed by handle_nav_event; everything else (mouse) goes to
    // RmlUi so clicking still works alongside controller navigation.
    SDL_Event ev;
    while (g_event_queue.try_dequeue(ev)) {
        if (g_doc != nullptr && handle_nav_event(g_doc, ev)) continue;
        RmlSDL::InputEventHandler(g_ui->context, ev);
    }

    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    g_ui->render_interface.start(list, w, h);
    if (w != g_ui->width || h != g_ui->height) {
        g_ui->width = w;
        g_ui->height = h;
        g_ui->context->SetDimensions(Rml::Vector2i(w, h));
    }
    g_ui->context->Update();
    g_ui->context->Render();
    g_ui->render_interface.end(list, swap_chain_framebuffer);
}

void deinit_hook() {
    if (g_ui) {
        g_ui->render_interface.reset();
        g_ui.reset();
    }
    Rml::Shutdown();
}

} // namespace

namespace pkmnstadium::ui_seam {

// Called once during startup (from rt64_render_context.cpp) to register the
// RmlUi overlay with RT64. Mirrors recompui::set_render_hooks().
void install() {
    RT64::SetRenderHooks(init_hook, draw_hook, deinit_hook);
    std::fprintf(stderr, "[ss-anne] render hooks installed\n");
    std::fflush(stderr);
}

// True while the launcher is the active screen (before PLAY). main.cpp uses
// this to decide whether to forward input to the launcher and to hold the boot.
bool launcher_visible() {
    return g_launcher_visible.load();
}

// True once the user has clicked PLAY. main.cpp's boot thread waits on this.
bool play_requested() {
    return g_play_requested.load();
}

// Enqueue an SDL event for the launcher to consume on the render thread.
// Safe to call from the gfx/event-pump thread.
void queue_event(const SDL_Event& ev) {
    g_event_queue.enqueue(ev);
}

// Hand the launcher the connected-gamepad list (instance id, name). Called by
// main.cpp after SDL gamecontroller init, before the render hooks initialize.
void set_gamepads(const std::vector<std::pair<int, std::string>>& pads) {
    g_gamepads = pads;
}

// Per-port (Player 1..4) assignment captured at PLAY. Read by main.cpp's boot
// thread to wire input routing + Transfer Pak presence.
PortAssignment port_assignment(int port) {
    PortAssignment pa;
    if (port < 0 || port > 3) return pa;
    pa.enabled = g_slot_enabled[port];
    const int dev = g_slot_device[port];
    if (dev == 0) {
        pa.kind = DeviceKind::None;
    } else if (dev == 1) {
        pa.kind = DeviceKind::Keyboard;
    } else {
        const int gi = dev - 2;
        if (gi >= 0 && gi < static_cast<int>(g_gamepads.size())) {
            pa.kind = DeviceKind::Gamepad;
            pa.gamepad_instance = g_gamepads[gi].first;
        }
    }
    return pa;
}

} // namespace pkmnstadium::ui_seam
