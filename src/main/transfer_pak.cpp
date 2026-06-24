// transfer_pak.cpp — PSR-specific Transfer Pak config glue.
//
// The Game Boy cartridge model (MBC1/3/5 + battery RAM + open-bus padding) and
// the libultra accessory HLE (__osContRamRead/Write, __osPfsGetStatus) now live
// in the shared engine: librecomp/src/gbcart.cpp + gbpak.cpp. This file used to
// carry its own copy of all of that; it was the origin those were promoted from,
// but the duplicate drifted (issue #17). PSR now uses the shared model, and this
// file is reduced to the only PSR-specific part: reading launcher.cfg (per-port
// GB ROM + save paths, with PSR_TRANSFER_PAK_P*_ROM/SAVE env overrides) and
// registering the carts with librecomp::gbcart.
#include "transfer_pak.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

#include "librecomp/gbcart.hpp"

#include "app_paths.h"

namespace pkmnstadium::transfer_pak {
namespace {
    constexpr int port_count = 4;

    std::once_flag config_once;

    std::string trim(std::string value) {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
            value.erase(value.begin());
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
            value.pop_back();
        }
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }
        return value;
    }

    std::string lower(std::string value) {
        for (char& ch : value) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return value;
    }

    std::unordered_map<std::string, std::string> read_config_file(
        const std::filesystem::path& path) {
        std::unordered_map<std::string, std::string> values;
        std::ifstream file(path);
        if (!file) {
            return values;
        }

        std::string line;
        while (std::getline(file, line)) {
            const size_t comment = line.find_first_of("#;");
            if (comment != std::string::npos) {
                line.resize(comment);
            }
            line = trim(line);
            if (line.empty() || line.front() == '[') {
                continue;
            }
            const size_t separator = line.find('=');
            if (separator == std::string::npos) {
                continue;
            }
            std::string key = lower(trim(line.substr(0, separator)));
            std::string value = trim(line.substr(separator + 1));
            if (!key.empty() && !value.empty()) {
                values[std::move(key)] = std::move(value);
            }
        }
        return values;
    }

    std::filesystem::path make_path(const std::string& value,
                                    const std::filesystem::path& base) {
        if (value.empty()) {
            return {};
        }
        std::filesystem::path path(value);
        return path.is_relative() ? base / path : path;
    }

    std::filesystem::path port_path(
        int port,
        const char* env_suffix,
        const char* config_suffix,
        const std::unordered_map<std::string, std::string>& config,
        const std::filesystem::path& config_base) {
        const std::string env_name =
            "PSR_TRANSFER_PAK_P" + std::to_string(port + 1) + "_" + env_suffix;
        if (const char* env = std::getenv(env_name.c_str()); env != nullptr && env[0] != '\0') {
            return make_path(env, std::filesystem::current_path());
        }

        const std::string config_name =
            "p" + std::to_string(port + 1) + "_" + config_suffix;
        auto it = config.find(config_name);
        return it == config.end() ? std::filesystem::path{} : make_path(it->second, config_base);
    }

    void load_config() {
        // Next to the exe, matching the launcher's writer (ui_seam.cpp) so the
        // emulated Transfer Pak reads the same launcher.cfg the GUI writes,
        // regardless of the process's working directory.
        const std::filesystem::path config_path =
            pkmnstadium::exe_dir() / "launcher.cfg";
        const auto config = read_config_file(config_path);

        for (int port = 0; port < port_count; port++) {
            const std::filesystem::path rom =
                port_path(port, "ROM", "rom", config, config_path.parent_path());
            const std::filesystem::path save =
                port_path(port, "SAVE", "save", config, config_path.parent_path());
            if (!rom.empty()) {
                // Hand off to the shared cart model; it loads the file, picks the
                // MBC, and logs "[gbcart] port N configured".
                librecomp::gbcart::set_cart(port, rom, save);
            }
        }
    }

    void ensure_configured() {
        std::call_once(config_once, load_config);
    }
}

void initialize() {
    ensure_configured();
}

bool has_transfer_pak(int port) {
    ensure_configured();
    return librecomp::gbcart::has_pak(port);
}
}
