#include <cstdint>
#include <cstdlib>

#include <ultramodern/ultramodern.hpp>

namespace {
constexpr uint32_t kRdramSize = 0x800000u;

bool normalize_audio_addr(uint32_t addr, uint32_t byte_count, uint32_t& kseg0_addr) {
    uint32_t paddr = 0;
    if (addr < kRdramSize) {
        paddr = addr;
    } else if (addr >= 0x80000000u && addr < 0x80000000u + kRdramSize) {
        paddr = addr - 0x80000000u;
    } else if (addr >= 0xA0000000u && addr < 0xA0000000u + kRdramSize) {
        paddr = addr - 0xA0000000u;
    } else {
        return false;
    }

    if (byte_count > kRdramSize || paddr > kRdramSize - byte_count) {
        return false;
    }

    kseg0_addr = 0x80000000u + paddr;
    return true;
}

bool gb_audio_enabled() {
    static const bool enabled = [] {
        const char* disable = std::getenv("PSR_DISABLE_GBTOWER_AUDIO");
        return !(disable != nullptr && disable[0] != '\0' && disable[0] != '0');
    }();
    return enabled;
}
} // namespace

extern "C" void pkmnstadium_gbtower_queue_audio(uint8_t* rdram,
                                                uint32_t audio_addr,
                                                uint32_t byte_count) {
    if (!gb_audio_enabled() || rdram == nullptr || byte_count == 0 || (byte_count & 1u) != 0) {
        return;
    }

    uint32_t kseg0_addr = 0;
    if (!normalize_audio_addr(audio_addr, byte_count, kseg0_addr)) {
        return;
    }

    ultramodern::queue_audio_buffer(rdram, static_cast<int32_t>(kseg0_addr), byte_count);
}
