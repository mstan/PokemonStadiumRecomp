#!/usr/bin/env python3
"""Static analyzer: enumerate every R_MIPS_32 reloc in fragment62
that targets the pattern bucket (0x8FF00000), and emit a census
table with: source offset (in fragment62), target offset (in pattern
variant), and the raw literal value.

This is the build-time complement to the runtime variant-probe in
extras.c — runtime only sees literals that flow through
Util_ConvertAddrToVirtAddr, which during attract is a small subset.
The static analyzer enumerates EVERY pattern-bucket reference in
fragment62's data, regardless of whether attract reaches them.

Stadium FRAGMENT reloc format:
- u32 count (big-endian) at fragment62_relocs ROM offset
- count * u32 entries: bits 31:24 = type, bits 19:0 = offset within fragment
- Types: 2=R_MIPS_32, 4=R_MIPS_26, 5=R_MIPS_HI16, 6=R_MIPS_LO16
"""

import struct
import sys
from pathlib import Path

ROM_PATH = Path("F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64")
RELOC_BIN = Path(
    "F:/Projects/n64recomp/PokemonStadiumRecomp/disasm/assets/us/fragments/62/"
    "fragment62_reloc.rodatabin.bin"
)
FRAGMENT62_ROM = 0x2EA8C0
FRAGMENT62_END = 0x378D00
FRAGMENT62_VRAM = 0x84300000

PATTERN_BUCKET = 0x8FF00000
PATTERN_BUCKET_MASK = 0xFFF00000


def main():
    rom = ROM_PATH.read_bytes()
    relocs = RELOC_BIN.read_bytes()

    body_size = FRAGMENT62_END - FRAGMENT62_ROM
    body = rom[FRAGMENT62_ROM:FRAGMENT62_END]

    n_relocs = struct.unpack(">I", relocs[0:4])[0]
    print(f"fragment62 body size = 0x{body_size:X} ({body_size} bytes)")
    print(f"fragment62 vram      = 0x{FRAGMENT62_VRAM:08X}")
    print(f"reloc count          = {n_relocs}")
    print()

    # Histogram by type
    type_count = {}
    pattern_relocs_32 = []
    pattern_relocs_hi16 = []
    pattern_relocs_lo16 = []

    # For HI/LO pairing we need to scan in order. We'll keep raw entries.
    raw = []
    for i in range(n_relocs):
        entry = struct.unpack(">I", relocs[4 + 4*i : 8 + 4*i])[0]
        rtype = (entry >> 24) & 0x7F
        offset = entry & 0x00FFFFF  # 20 bits per recompiler code
        raw.append((rtype, offset))
        type_count[rtype] = type_count.get(rtype, 0) + 1

    print("Reloc type histogram:")
    for t, c in sorted(type_count.items()):
        name = {2: "R_MIPS_32", 4: "R_MIPS_26", 5: "R_MIPS_HI16",
                6: "R_MIPS_LO16"}.get(t, f"unknown({t})")
        print(f"  type {t} ({name}): {c}")
    print()

    # R_MIPS_32: read 32-bit literal at offset, check bucket
    for i, (rtype, offset) in enumerate(raw):
        if offset + 4 > body_size:
            continue
        word = struct.unpack(">I", body[offset:offset+4])[0]
        if rtype == 2 and (word & PATTERN_BUCKET_MASK) == PATTERN_BUCKET:
            target_offset = word & 0x000FFFFF
            pattern_relocs_32.append((offset, word, target_offset))
        elif rtype == 5:
            # HI16: lookup paired LO16 to compute combined target
            hi_imm = word & 0xFFFF
            # find next LO16 in raw list
            j = i + 1
            while j < len(raw) and raw[j][0] != 6:
                j += 1
            if j < len(raw):
                lo_offset = raw[j][1]
                if lo_offset + 4 <= body_size:
                    lo_word = struct.unpack(
                        ">I", body[lo_offset:lo_offset+4])[0]
                    lo_imm = lo_word & 0xFFFF
                    # Sign-extend lo
                    if lo_imm & 0x8000:
                        lo_imm = lo_imm - 0x10000
                    combined = (hi_imm << 16) + lo_imm
                    if (combined & PATTERN_BUCKET_MASK) == PATTERN_BUCKET:
                        target_offset = combined & 0x000FFFFF
                        pattern_relocs_hi16.append(
                            (offset, lo_offset, combined, target_offset))

    print(f"R_MIPS_32 relocs targeting pattern bucket 0x{PATTERN_BUCKET:08X}: "
          f"{len(pattern_relocs_32)}")
    print()
    if pattern_relocs_32:
        # Group by target_offset for census
        by_offset = {}
        for src, lit, off in pattern_relocs_32:
            by_offset.setdefault(off, []).append((src, lit))
        print(f"Distinct target offsets (R_MIPS_32): {len(by_offset)}")
        for off in sorted(by_offset.keys()):
            srcs = by_offset[off]
            print(f"  offset=0x{off:05X} literal=0x{srcs[0][1]:08X} "
                  f"({len(srcs)} sources, first source_offset=0x{srcs[0][0]:05X})")
    print()
    print(f"R_MIPS_HI16/LO16 pairs targeting pattern bucket: "
          f"{len(pattern_relocs_hi16)}")
    if pattern_relocs_hi16:
        by_offset = {}
        for hi_src, lo_src, lit, off in pattern_relocs_hi16:
            by_offset.setdefault(off, []).append((hi_src, lo_src))
        print(f"Distinct target offsets (HI/LO): {len(by_offset)}")
        for off in sorted(by_offset.keys()):
            srcs = by_offset[off]
            print(f"  offset=0x{off:05X} literal=0x{(PATTERN_BUCKET|off):08X} "
                  f"({len(srcs)} pairs)")


if __name__ == "__main__":
    main()
