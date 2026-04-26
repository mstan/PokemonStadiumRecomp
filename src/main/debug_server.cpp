/*
 * debug_server.cpp — Minimal TCP debug server for PokemonStadiumRecomp.
 *
 * Modeled after nesrecomp's runner/src/debug_server.c (per
 * F:\Projects\recomp-template\NES\TCP.md). Default port 4370,
 * single-client, JSON line protocol.
 *
 * Commands implemented for first-pass visibility:
 *
 *   ping                         → {"ok":true,"pong":true}
 *   status                       → frame counter, vi count, fast_forward state, errors
 *   set_button {name, down}      → simulate controller button press
 *   set_stick {x, y}             → analog stick override (-128..127)
 *   clear_input                  → drop overrides
 *   fast_forward {on}            → flip fast-forward state
 *   enable_instant_present       → tell the renderer to skip vsync
 *   screenshot {path}            → ask the renderer to dump the next framebuffer
 *   tail_errlog                  → returns last_error.log content (post-mortem inspection)
 *   quit                         → exit cleanly
 *
 * Per project principles (F:\Projects\recomp-template\NES\TCP.md):
 * any state we want visible from outside grows TCP commands here.
 * Don't add side-channel logging.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include "debug_server.h"

#pragma comment(lib, "ws2_32.lib")

namespace pkmnstadium {
namespace dbg {

// ---- Public state polled by the runner each frame --------------------------
std::atomic<bool>     g_fast_forward{false};
std::atomic<bool>     g_enable_instant_present_request{false};
std::atomic<uint64_t> g_vi_ticks{0};
std::atomic<uint64_t> g_frame_count{0};

// Button override surface (controller 0). When `g_input_override_active` is
// true, the runner uses these in place of SDL polling.
std::atomic<bool>     g_input_override_active{false};
std::atomic<uint16_t> g_buttons_override{0};
std::atomic<int>      g_stick_x_override{0};  // -128..127
std::atomic<int>      g_stick_y_override{0};

std::atomic<uint64_t> g_send_dl_count{0};
std::atomic<uint64_t> g_update_screen_count{0};
std::atomic<uint64_t> g_send_dl_audio_count{0};
std::atomic<uint64_t> g_send_dl_gfx_count{0};
std::atomic<uint64_t> g_send_dl_other_count{0};

// ---- Internals -------------------------------------------------------------
static std::thread        s_thread;
static std::atomic<bool>  s_running{false};
static SOCKET             s_listen_sock = INVALID_SOCKET;

// Map button name → N64 contStat bit.
static uint16_t button_bit(const std::string& n) {
    if (n == "A")        return 0x8000;
    if (n == "B")        return 0x4000;
    if (n == "Z")        return 0x2000;
    if (n == "Start")    return 0x1000;
    if (n == "DUp")      return 0x0800;
    if (n == "DDown")    return 0x0400;
    if (n == "DLeft")    return 0x0200;
    if (n == "DRight")   return 0x0100;
    if (n == "L")        return 0x0020;
    if (n == "R")        return 0x0010;
    if (n == "CUp")      return 0x0008;
    if (n == "CDown")    return 0x0004;
    if (n == "CLeft")    return 0x0002;
    if (n == "CRight")   return 0x0001;
    return 0;
}

// Crude JSON value extraction — enough for our small command surface.
// We do not pull in a JSON library to avoid one more dep.
static std::string get_str(const std::string& body, const char* key) {
    std::string needle = std::string("\"") + key + "\"";
    size_t k = body.find(needle);
    if (k == std::string::npos) return {};
    size_t colon = body.find(':', k);
    if (colon == std::string::npos) return {};
    size_t qa = body.find('"', colon);
    if (qa == std::string::npos) return {};
    size_t qb = body.find('"', qa + 1);
    if (qb == std::string::npos) return {};
    return body.substr(qa + 1, qb - qa - 1);
}

static int get_int(const std::string& body, const char* key, int dflt) {
    std::string needle = std::string("\"") + key + "\"";
    size_t k = body.find(needle);
    if (k == std::string::npos) return dflt;
    size_t colon = body.find(':', k);
    if (colon == std::string::npos) return dflt;
    return std::atoi(body.c_str() + colon + 1);
}

static bool get_bool(const std::string& body, const char* key, bool dflt) {
    std::string needle = std::string("\"") + key + "\"";
    size_t k = body.find(needle);
    if (k == std::string::npos) return dflt;
    size_t colon = body.find(':', k);
    if (colon == std::string::npos) return dflt;
    // Skip whitespace.
    const char* p = body.c_str() + colon + 1;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "true", 4) == 0)  return true;
    if (strncmp(p, "false", 5) == 0) return false;
    return std::atoi(p) != 0;
}

static std::string handle_command(const std::string& line) {
    auto cmd = get_str(line, "cmd");
    if (cmd.empty()) {
        // Bare command (no JSON) — match against the line trimmed.
        std::string bare = line;
        while (!bare.empty() && (bare.back() == '\n' || bare.back() == '\r' || bare.back() == ' ')) bare.pop_back();
        cmd = bare;
    }

    if (cmd == "ping") {
        return R"({"ok":true,"pong":true})";
    }
    if (cmd == "status") {
        char buf[1024];
        std::snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"frame\":%llu,\"vi\":%llu,\"fast_forward\":%s,\"input_override\":%s,\"buttons\":%u,\"sx\":%d,\"sy\":%d,"
            "\"send_dl\":%llu,\"send_dl_gfx\":%llu,\"send_dl_audio\":%llu,\"send_dl_other\":%llu,\"update_screen\":%llu}",
            (unsigned long long)g_frame_count.load(),
            (unsigned long long)g_vi_ticks.load(),
            g_fast_forward.load() ? "true" : "false",
            g_input_override_active.load() ? "true" : "false",
            (unsigned)g_buttons_override.load(),
            g_stick_x_override.load(),
            g_stick_y_override.load(),
            (unsigned long long)g_send_dl_count.load(),
            (unsigned long long)g_send_dl_gfx_count.load(),
            (unsigned long long)g_send_dl_audio_count.load(),
            (unsigned long long)g_send_dl_other_count.load(),
            (unsigned long long)g_update_screen_count.load()
        );
        return buf;
    }
    if (cmd == "set_button") {
        auto name = get_str(line, "name");
        bool down = get_bool(line, "down", true);
        uint16_t bit = button_bit(name);
        if (bit == 0) return R"({"ok":false,"error":"unknown button name"})";
        g_input_override_active.store(true);
        uint16_t cur = g_buttons_override.load();
        if (down) cur |= bit; else cur &= ~bit;
        g_buttons_override.store(cur);
        return R"({"ok":true})";
    }
    if (cmd == "set_stick") {
        g_input_override_active.store(true);
        g_stick_x_override.store(get_int(line, "x", 0));
        g_stick_y_override.store(get_int(line, "y", 0));
        return R"({"ok":true})";
    }
    if (cmd == "clear_input") {
        g_input_override_active.store(false);
        g_buttons_override.store(0);
        g_stick_x_override.store(0);
        g_stick_y_override.store(0);
        return R"({"ok":true})";
    }
    if (cmd == "fast_forward") {
        g_fast_forward.store(get_bool(line, "on", true));
        return R"({"ok":true})";
    }
    if (cmd == "enable_instant_present") {
        g_enable_instant_present_request.store(true);
        return R"({"ok":true})";
    }
    if (cmd == "tail_errlog") {
        FILE* f = fopen("F:/Projects/PokemonStadiumRecomp/build/last_error.log", "rb");
        if (!f) return R"({"ok":true,"errlog":""})";
        char chunk[4096]{};
        size_t n = fread(chunk, 1, sizeof(chunk) - 1, f);
        chunk[n] = 0;
        fclose(f);
        // Naive escape — replace " and \ with safe substitutes.
        std::string out = R"({"ok":true,"errlog":")";
        for (size_t i = 0; i < n; i++) {
            char c = chunk[i];
            if (c == '"' || c == '\\') { out += '\\'; out += c; }
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if ((unsigned char)c < 0x20) ; // skip
            else out += c;
        }
        out += R"("})";
        return out;
    }
    if (cmd == "quit") {
        ExitProcess(0);  // hard exit — debug-driven shutdown
        return R"({"ok":true})";
    }
    return R"({"ok":false,"error":"unknown command"})";
}

static void server_loop(int port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[dbg] WSAStartup failed\n");
        return;
    }
    s_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_listen_sock == INVALID_SOCKET) {
        fprintf(stderr, "[dbg] socket() failed\n");
        return;
    }

    BOOL reuse = TRUE;
    setsockopt(s_listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
    if (bind(s_listen_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "[dbg] bind() failed err=%d\n", WSAGetLastError());
        return;
    }
    if (listen(s_listen_sock, 1) == SOCKET_ERROR) {
        fprintf(stderr, "[dbg] listen() failed\n");
        return;
    }
    fprintf(stderr, "[dbg] listening on 127.0.0.1:%d\n", port);
    fflush(stderr);

    while (s_running.load()) {
        sockaddr_in caddr{};
        int caddr_len = sizeof(caddr);
        SOCKET client = accept(s_listen_sock, (sockaddr*)&caddr, &caddr_len);
        if (client == INVALID_SOCKET) {
            if (!s_running.load()) break;
            continue;
        }
        // Read line-by-line until disconnect.
        std::string buf;
        buf.reserve(1024);
        char chunk[1024];
        while (s_running.load()) {
            int n = recv(client, chunk, sizeof(chunk), 0);
            if (n <= 0) break;
            buf.append(chunk, chunk + n);
            // Process complete lines.
            size_t nl;
            while ((nl = buf.find('\n')) != std::string::npos) {
                std::string line = buf.substr(0, nl);
                buf.erase(0, nl + 1);
                std::string resp = handle_command(line);
                resp += "\n";
                send(client, resp.c_str(), (int)resp.size(), 0);
            }
        }
        closesocket(client);
    }

    closesocket(s_listen_sock);
    WSACleanup();
}

void start(int port) {
    if (s_running.load()) return;
    s_running.store(true);
    s_thread = std::thread(server_loop, port);
    s_thread.detach();
}

void shutdown() {
    s_running.store(false);
    if (s_listen_sock != INVALID_SOCKET) {
        closesocket(s_listen_sock);
    }
}

} // namespace dbg
} // namespace pkmnstadium
