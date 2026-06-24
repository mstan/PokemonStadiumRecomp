#pragma once

#include <cstdint>

namespace pkmnstadium::transfer_pak {
    constexpr int block_size = 32;

    void initialize();
    bool has_transfer_pak(int port);
    int read_block(int port, uint16_t block_address, uint8_t* out);
    int write_block(int port, uint16_t block_address, const uint8_t* data);

    // Dump the always-on block-I/O ring (summary: which switchable ROM banks were
    // read + max; tail: last `tail` entries) to `path`. Diagnoses the GB-Tower
    // full-cart read (issue #17).
    void ring_dump(const char* path, int tail);
}
