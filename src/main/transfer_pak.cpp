#include "transfer_pak.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <librecomp/helpers.hpp>

#include "ultramodern/ultramodern.hpp"
#include "app_paths.h"

namespace pkmnstadium::transfer_pak {
namespace {
    constexpr int port_count = 4;
    constexpr int pfs_err_no_pak = 1;
    constexpr int pfs_err_invalid = 5;
    constexpr uint16_t transfer_pak_address_mask = 0x7FFF;
    constexpr uint16_t transfer_pak_cart_window = 0x4000;

    enum class Mbc {
        RomOnly,
        Mbc1,
        Mbc3,
        Mbc5,
        Unsupported,
    };

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

    std::vector<uint8_t> read_file(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return {};
        }
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    size_t header_ram_size(uint8_t code) {
        switch (code) {
        case 0x00: return 0;
        case 0x01: return 2 * 1024;
        case 0x02: return 8 * 1024;
        case 0x03: return 32 * 1024;
        case 0x04: return 128 * 1024;
        case 0x05: return 64 * 1024;
        default: return 0;
        }
    }

    struct Cartridge {
        std::filesystem::path rom_path;
        std::filesystem::path save_path;
        std::vector<uint8_t> rom;
        std::vector<uint8_t> ram;
        Mbc mbc = Mbc::Unsupported;
        bool ram_enabled = false;
        bool dirty = false;
        uint8_t rom_bank_low = 1;
        uint8_t bank_high = 0;
        uint8_t ram_bank = 0;
        uint8_t mbc1_mode = 0;
        uint16_t mbc5_rom_bank = 1;

        bool load(const std::filesystem::path& configured_rom,
                  const std::filesystem::path& configured_save) {
            rom_path = configured_rom;
            save_path = configured_save;
            rom = read_file(rom_path);
            if (rom.size() < 0x150) {
                std::fprintf(stderr, "[transfer-pak] ROM is missing or too small: %s\n",
                             rom_path.string().c_str());
                return false;
            }

            const uint8_t type = rom[0x147];
            if (type == 0x00 || type == 0x08 || type == 0x09) {
                mbc = Mbc::RomOnly;
            } else if (type >= 0x01 && type <= 0x03) {
                mbc = Mbc::Mbc1;
            } else if (type >= 0x0F && type <= 0x13) {
                mbc = Mbc::Mbc3;
            } else if (type >= 0x19 && type <= 0x1E) {
                mbc = Mbc::Mbc5;
            } else {
                std::fprintf(stderr,
                             "[transfer-pak] unsupported Game Boy cart type 0x%02X: %s\n",
                             type, rom_path.string().c_str());
                return false;
            }

            if (!save_path.empty()) {
                ram = read_file(save_path);
            }
            const size_t declared_ram = header_ram_size(rom[0x149]);
            if (ram.size() < declared_ram) {
                ram.resize(declared_ram, 0xFF);
            }

            std::fprintf(stderr,
                         "[transfer-pak] loaded ROM %s (%zu bytes, type 0x%02X), save %s (%zu bytes)\n",
                         rom_path.string().c_str(), rom.size(), type,
                         save_path.empty() ? "(none)" : save_path.string().c_str(), ram.size());
            return true;
        }

        size_t rom_index(size_t bank, uint16_t address_in_bank) const {
            if (rom.empty()) {
                return 0;
            }
            const size_t banks = (rom.size() + 0x3FFF) / 0x4000;
            bank %= banks;
            return bank * 0x4000 + address_in_bank;
        }

        size_t selected_switchable_rom_bank() const {
            if (mbc == Mbc::Mbc5) {
                return mbc5_rom_bank;
            }
            if (mbc == Mbc::Mbc1) {
                size_t bank = (rom_bank_low & 0x1F) | ((bank_high & 0x03) << 5);
                if ((bank & 0x1F) == 0) {
                    bank++;
                }
                return bank;
            }
            size_t bank = rom_bank_low & 0x7F;
            return bank == 0 ? 1 : bank;
        }

        size_t selected_ram_offset(uint16_t address) const {
            const size_t offset = address - 0xA000;
            const size_t selected_bank =
                (mbc == Mbc::Mbc1 && mbc1_mode == 0) ? 0 : (ram_bank & 0x03);
            return selected_bank * 0x2000 + offset;
        }

        uint8_t read(uint16_t address) const {
            if (address < 0x4000) {
                size_t bank = 0;
                if (mbc == Mbc::Mbc1 && mbc1_mode != 0) {
                    bank = (bank_high & 0x03) << 5;
                }
                const size_t index = rom_index(bank, address);
                return index < rom.size() ? rom[index] : 0xFF;
            }
            if (address < 0x8000) {
                const size_t index = rom_index(selected_switchable_rom_bank(), address - 0x4000);
                return index < rom.size() ? rom[index] : 0xFF;
            }
            if (address >= 0xA000 && address < 0xC000 && ram_enabled) {
                const size_t index = selected_ram_offset(address);
                return index < ram.size() ? ram[index] : 0xFF;
            }
            return 0xFF;
        }

        void write(uint16_t address, uint8_t value) {
            if (mbc == Mbc::RomOnly) {
                if (address >= 0xA000 && address < 0xC000 && !ram.empty()) {
                    const size_t index = address - 0xA000;
                    if (index < ram.size() && ram[index] != value) {
                        ram[index] = value;
                        dirty = true;
                    }
                }
                return;
            }

            if (address < 0x2000) {
                ram_enabled = (value & 0x0F) == 0x0A;
                return;
            }
            if (address < 0x4000) {
                if (mbc == Mbc::Mbc1) {
                    rom_bank_low = value & 0x1F;
                    if (rom_bank_low == 0) {
                        rom_bank_low = 1;
                    }
                } else if (mbc == Mbc::Mbc5) {
                    if (address < 0x3000) {
                        mbc5_rom_bank = static_cast<uint16_t>(
                            (mbc5_rom_bank & 0x100) | value);
                    } else {
                        mbc5_rom_bank = static_cast<uint16_t>(
                            (mbc5_rom_bank & 0x0FF) | ((value & 1) << 8));
                    }
                } else {
                    rom_bank_low = value & 0x7F;
                    if (rom_bank_low == 0) {
                        rom_bank_low = 1;
                    }
                }
                return;
            }
            if (address < 0x6000) {
                if (mbc == Mbc::Mbc1) {
                    bank_high = value & 0x03;
                    ram_bank = bank_high;
                } else if (mbc == Mbc::Mbc5) {
                    ram_bank = value & 0x0F;
                } else if (value <= 0x03) {
                    ram_bank = value;
                }
                return;
            }
            if (address < 0x8000) {
                if (mbc == Mbc::Mbc1) {
                    mbc1_mode = value & 1;
                }
                return;
            }
            if (address >= 0xA000 && address < 0xC000 && ram_enabled) {
                const size_t index = selected_ram_offset(address);
                if (index < ram.size() && ram[index] != value) {
                    ram[index] = value;
                    dirty = true;
                }
            }
        }

        void flush_save() {
            if (!dirty || save_path.empty()) {
                return;
            }
            std::ofstream file(save_path, std::ios::binary | std::ios::trunc);
            if (!file) {
                std::fprintf(stderr, "[transfer-pak] failed to write save: %s\n",
                             save_path.string().c_str());
                return;
            }
            file.write(reinterpret_cast<const char*>(ram.data()),
                       static_cast<std::streamsize>(ram.size()));
            if (file) {
                dirty = false;
            }
        }
    };

    struct Port {
        bool configured = false;
        bool pak_enabled = false;
        bool cart_enabled = false;
        uint8_t address_bank = 3;
        uint8_t reset_state = 0;
        Cartridge cart;

        uint8_t status() {
            uint8_t value = 0;
            value |= cart_enabled ? 0x01 : 0;
            value |= static_cast<uint8_t>((reset_state & 0x03) << 2);
            value |= configured ? 0 : 0x40;
            value |= pak_enabled ? 0x80 : 0;

            if (cart_enabled && reset_state == 3) {
                reset_state = 2;
            } else if (!cart_enabled && reset_state == 2) {
                reset_state = 1;
            } else if (!cart_enabled && reset_state == 1) {
                reset_state = 0;
            }
            return value;
        }

        uint8_t read(uint16_t accessory_address) {
            accessory_address &= transfer_pak_address_mask;
            if (!pak_enabled) {
                return 0;
            }
            if (accessory_address <= 0x1FFF) {
                return 0x84;
            }
            if (accessory_address <= 0x2FFF) {
                return address_bank;
            }
            if (accessory_address <= 0x3FFF) {
                return status();
            }
            if (!cart_enabled) {
                return 0;
            }
            const uint16_t bus_address = static_cast<uint16_t>(
                transfer_pak_cart_window * address_bank +
                accessory_address - transfer_pak_cart_window);
            return cart.read(bus_address);
        }

        void write(uint16_t accessory_address, uint8_t value) {
            accessory_address &= transfer_pak_address_mask;
            if (accessory_address <= 0x1FFF) {
                const bool was_enabled = pak_enabled;
                if (value == 0x84) {
                    pak_enabled = true;
                } else if (value == 0xFE) {
                    pak_enabled = false;
                }
                if (!was_enabled && pak_enabled) {
                    address_bank = 3;
                    cart_enabled = false;
                    reset_state = 0;
                }
                return;
            }
            if (!pak_enabled) {
                return;
            }
            if (accessory_address <= 0x2FFF) {
                address_bank = value > 3 ? 0 : value;
                return;
            }
            if (accessory_address <= 0x3FFF) {
                const bool was_enabled = cart_enabled;
                cart_enabled = (value & 1) != 0;
                if (!was_enabled && cart_enabled) {
                    reset_state = 3;
                }
                return;
            }
            if (!cart_enabled) {
                return;
            }
            const uint16_t bus_address = static_cast<uint16_t>(
                transfer_pak_cart_window * address_bank +
                accessory_address - transfer_pak_cart_window);
            cart.write(bus_address, value);
        }
    };

    std::array<Port, port_count> ports;
    std::once_flag config_once;
    std::mutex port_mutex;
    bool debug_enabled = false;

    // ---- Always-on block-I/O ring (matches librecomp gbcart's ring) -----------
    // PSR's GB-Tower reads the GB cart through THIS app-level Transfer Pak HLE
    // (not librecomp::gbcart), so the gbcart ring stays empty. Record every
    // read_block/write_block with the cart's selected ROM bank + the mapped GB
    // bus address so a probe can see exactly which ROM banks the full-cart read
    // walked and where it stopped (issue #17: Red/Blue stall — data ends at bank
    // 44, Yellow fills 64). Caller holds port_mutex.
    struct TpakRingEntry {
        uint32_t seq;
        uint16_t block_addr;   // libultra block address (x32 = accessory byte addr)
        uint16_t bus_addr;     // mapped GB bus address (0x0000-0xFFFF)
        uint16_t rom_bank;     // cart's currently-selected switchable ROM bank
        uint8_t  port;
        uint8_t  kind;         // 0 = read, 1 = write
        uint8_t  addr_bank;    // Transfer Pak bank register (0..3)
        uint8_t  flags;        // bit0 pak_enabled, bit1 cart_enabled, bit2 ram_enabled
        uint8_t  b0;           // first data byte read/written
    };
    constexpr size_t tpak_ring_cap = 65536;
    TpakRingEntry g_tpak_ring[tpak_ring_cap];
    uint32_t g_tpak_ring_seq = 0;  // total appended; guarded by port_mutex

    void tpak_ring_append(int port, uint8_t kind, uint16_t block_address, uint8_t b0) {
        const Port& p = ports[port];
        const uint16_t byte_address = static_cast<uint16_t>(block_address * block_size);
        const uint16_t masked = byte_address & transfer_pak_address_mask;
        const uint16_t bus_addr = (masked > 0x3FFF)
            ? static_cast<uint16_t>(transfer_pak_cart_window * p.address_bank +
                                    masked - transfer_pak_cart_window)
            : masked;
        TpakRingEntry& e = g_tpak_ring[g_tpak_ring_seq % tpak_ring_cap];
        e.seq = g_tpak_ring_seq;
        e.block_addr = block_address;
        e.bus_addr = bus_addr;
        e.rom_bank = static_cast<uint16_t>(p.cart.selected_switchable_rom_bank());
        e.port = static_cast<uint8_t>(port);
        e.kind = kind;
        e.addr_bank = p.address_bank;
        e.flags = static_cast<uint8_t>((p.pak_enabled ? 1 : 0) | (p.cart_enabled ? 2 : 0) |
                                       (p.cart.ram_enabled ? 4 : 0));
        e.b0 = b0;
        g_tpak_ring_seq++;
    }

    uint32_t si_dma_latency_us() {
        static uint32_t value = []() {
            constexpr uint32_t default_latency_us = 50;
            const char* env = std::getenv("N64RECOMP_SI_DMA_US");
            if (env == nullptr || env[0] == '\0') {
                return default_latency_us;
            }
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(env, &end, 10);
            if (end == env) {
                return default_latency_us;
            }
            return static_cast<uint32_t>(parsed);
        }();
        return value;
    }

    void wait_for_si_dma(uint8_t* rdram, PTR(OSMesgQueue) mq) {
        const uint32_t latency_us = si_dma_latency_us();
        if (latency_us == 0 || mq == NULLPTR || !ultramodern::is_game_thread()) {
            return;
        }

        if (ultramodern::thread_queue_empty(PASS_RDRAM ultramodern::running_queue)) {
            ultramodern::sleep_milliseconds((latency_us + 999) / 1000);
            return;
        }

        ultramodern::send_external_message_after(PASS_RDRAM mq, (OSMesg)0, latency_us);
        osRecvMesg(PASS_RDRAM mq, NULLPTR, OS_MESG_BLOCK);
    }

    void wait_for_cont_ram_transfer(uint8_t* rdram, PTR(OSMesgQueue) mq) {
        wait_for_si_dma(rdram, mq);
        wait_for_si_dma(rdram, mq);
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
        if (const char* dbg = std::getenv("PSR_TRANSFER_PAK_DEBUG");
            dbg != nullptr && dbg[0] != '\0' && dbg[0] != '0') {
            debug_enabled = true;
        }

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
                ports[port].configured = ports[port].cart.load(rom, save);
                if (ports[port].configured) {
                    std::fprintf(stderr, "[transfer-pak] port %d configured\n", port + 1);
                }
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
    std::scoped_lock lock(port_mutex);
    return port >= 0 && port < port_count && ports[port].configured;
}

int read_block(int port, uint16_t block_address, uint8_t* out) {
    ensure_configured();
    std::scoped_lock lock(port_mutex);
    if (port < 0 || port >= port_count || !ports[port].configured) {
        return pfs_err_no_pak;
    }
    if (out == nullptr) {
        return pfs_err_invalid;
    }
    const uint16_t byte_address = static_cast<uint16_t>(block_address * block_size);
    for (int i = 0; i < block_size; i++) {
        out[i] = ports[port].read(static_cast<uint16_t>(byte_address + i));
    }
    tpak_ring_append(port, 0, block_address, out[0]);
    if (debug_enabled) {
        static int header_read_logs = 0;
        if (block_address >= 0x600 && block_address < 0x610 && header_read_logs++ < 16) {
            std::fprintf(stderr, "[transfer-pak] read port %d block 0x%03X:", port + 1, block_address);
            for (int i = 0; i < block_size; i++) {
                std::fprintf(stderr, " %02X", out[i]);
            }
            std::fprintf(stderr, "\n");
        }
    }
    return 0;
}

int write_block(int port, uint16_t block_address, const uint8_t* data) {
    ensure_configured();
    std::scoped_lock lock(port_mutex);
    if (port < 0 || port >= port_count || !ports[port].configured) {
        return pfs_err_no_pak;
    }
    if (data == nullptr) {
        return pfs_err_invalid;
    }
    if (debug_enabled) {
        static int control_write_logs = 0;
        if ((block_address == 0x400 || block_address == 0x500 || block_address == 0x580) &&
            control_write_logs++ < 32) {
            std::fprintf(stderr,
                         "[transfer-pak] write port %d block 0x%03X data[0]=0x%02X data[31]=0x%02X\n",
                         port + 1, block_address, data[0], data[block_size - 1]);
        }
    }
    const uint16_t byte_address = static_cast<uint16_t>(block_address * block_size);
    for (int i = 0; i < block_size; i++) {
        ports[port].write(static_cast<uint16_t>(byte_address + i), data[i]);
    }
    tpak_ring_append(port, 1, block_address, data[0]);
    ports[port].cart.flush_save();
    return 0;
}

void ring_dump(const char* path, int tail) {
    std::scoped_lock lock(port_mutex);
    std::ofstream f(path, std::ios::trunc);
    if (!f) {
        return;
    }
    const uint32_t total = g_tpak_ring_seq;
    const uint32_t avail = total < tpak_ring_cap ? total : static_cast<uint32_t>(tpak_ring_cap);
    if (tail <= 0 || static_cast<uint32_t>(tail) > avail) {
        tail = static_cast<int>(avail);
    }
    uint32_t reads = 0, writes = 0, max_bank = 0, max_bus = 0;
    uint64_t bank_seen = 0;  // bitmask of switchable ROM banks 0..63 touched by cart-window reads
    const uint32_t start = total > tpak_ring_cap ? total - static_cast<uint32_t>(tpak_ring_cap) : 0;
    for (uint32_t s = start; s < total; s++) {
        const TpakRingEntry& e = g_tpak_ring[s % tpak_ring_cap];
        if (e.kind == 0) {
            reads++;
            if (e.bus_addr >= 0x4000 && e.bus_addr < 0x8000) {
                if (e.rom_bank < 64) bank_seen |= (1ull << e.rom_bank);
                if (e.rom_bank > max_bank) max_bank = e.rom_bank;
            }
            if (e.bus_addr > max_bus) max_bus = e.bus_addr;
        } else {
            writes++;
        }
    }
    f << "=== transfer_pak block-I/O ring (total=" << total << " avail=" << avail
      << " reads=" << reads << " writes=" << writes << ") ===\n";
    f << "switchable_rom_banks_read (max=" << max_bank << "): ";
    for (int b = 0; b < 64; b++) {
        if (bank_seen & (1ull << b)) f << b << ' ';
    }
    f << "\nmax cart-window bus_addr=0x" << std::hex << max_bus << std::dec << "\n";
    f << "--- last " << tail << " entries (blk=libultra block, bus=GB addr, rb=rom bank;"
         " k=0 read/1 write; P/C/R=pak/cart/ram enabled) ---\n";
    f << "seq port k blk bus rb tpb b0 PCR\n";
    const uint32_t first = (static_cast<uint32_t>(tail) >= avail) ? start : (total - static_cast<uint32_t>(tail));
    for (uint32_t s = first; s < total; s++) {
        const TpakRingEntry& e = g_tpak_ring[s % tpak_ring_cap];
        f << e.seq << ' ' << int(e.port) << ' ' << int(e.kind)
          << " 0x" << std::hex << e.block_addr << " 0x" << e.bus_addr << std::dec
          << ' ' << e.rom_bank << ' ' << int(e.addr_bank)
          << " 0x" << std::hex << int(e.b0) << std::dec
          << ' ' << ((e.flags & 1) ? 'P' : '-') << ((e.flags & 2) ? 'C' : '-')
          << ((e.flags & 4) ? 'R' : '-') << '\n';
    }
}
}

extern "C" void __osContRamRead_recomp(uint8_t* rdram, recomp_context* ctx) {
    const PTR(OSMesgQueue) mq = _arg<0, PTR(OSMesgQueue)>(rdram, ctx);
    const int channel = _arg<1, s32>(rdram, ctx);
    const uint16_t address = _arg<2, u16>(rdram, ctx);
    const gpr buffer = ctx->r7;
    std::array<uint8_t, pkmnstadium::transfer_pak::block_size> block{};

    pkmnstadium::transfer_pak::wait_for_cont_ram_transfer(rdram, mq);

    const int ret = pkmnstadium::transfer_pak::read_block(channel, address, block.data());
    if (ret == 0) {
        for (int i = 0; i < pkmnstadium::transfer_pak::block_size; i++) {
            MEM_B(i, buffer) = static_cast<int8_t>(block[i]);
        }
    }
    _return<s32>(ctx, ret);
}

extern "C" void __osContRamWrite_recomp(uint8_t* rdram, recomp_context* ctx) {
    const PTR(OSMesgQueue) mq = _arg<0, PTR(OSMesgQueue)>(rdram, ctx);
    const int channel = _arg<1, s32>(rdram, ctx);
    const uint16_t address = _arg<2, u16>(rdram, ctx);
    const gpr buffer = ctx->r7;
    std::array<uint8_t, pkmnstadium::transfer_pak::block_size> block{};

    for (int i = 0; i < pkmnstadium::transfer_pak::block_size; i++) {
        block[i] = MEM_BU(i, buffer);
    }
    pkmnstadium::transfer_pak::wait_for_cont_ram_transfer(rdram, mq);

    const int ret = pkmnstadium::transfer_pak::write_block(channel, address, block.data());
    _return<s32>(ctx, ret);
}

extern "C" void __osPfsGetStatus_recomp(uint8_t* rdram, recomp_context* ctx) {
    const PTR(OSMesgQueue) mq = _arg<0, PTR(OSMesgQueue)>(rdram, ctx);
    const int channel = _arg<1, s32>(rdram, ctx);
    pkmnstadium::transfer_pak::wait_for_cont_ram_transfer(rdram, mq);
    _return<s32>(ctx, pkmnstadium::transfer_pak::has_transfer_pak(channel) ? 0 : 1);
}
