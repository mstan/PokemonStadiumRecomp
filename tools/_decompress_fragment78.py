#!/usr/bin/env python3
"""Decompress fragment78 from its PERS-SZP/Yay0 wrapper in baserom.z64.

fragment78 (link vram 0x8FF00000, runtime id=0xEF) is NOT loaded from
ROM 0x1206400 as our prior split assumed. It is CPU-decompressed at
runtime from a Yay0 stream wrapped in a PERS-SZP header located at
ROM 0x9E93F0..0x9EF????.

Wrapper layout:
    ROM 0x9E93F0   "PERS-SZP\0"               (8 bytes magic)
    ROM 0x9E93F8   uint32 BE  payload_off=0x18
    ROM 0x9E93FC   uint32 BE  decomp_size=0x25340
    ROM 0x9E9400   uint32 BE  decomp_size=0x25340  (duplicated)
    ROM 0x9E9404   uint32 BE  reserved (0)
    ROM 0x9E9408   "Yay0"                     (Yay0 magic, payload begins)
    ROM 0x9E940C   uint32 BE  decomp_size=0x25340
    ROM 0x9E9410   uint32 BE  link_table_off=0x2280
    ROM 0x9E9414   uint32 BE  data_off=0x63A4
    ROM 0x9E9418+  yay0 control bitstream + link table + literal data

Decompressed output (0x25340 bytes) is the canonical fragment78 binary
matching Stadium's runtime layout exactly:
    +0x00      J 0x8FF00020 (link-time trampoline)
    +0x08      "FRAGMENT" magic
    +0x14      relocOffset = 0x24E40
    +0x1C      sizeInRam   = 0x24E40
    +0x20..    code (func_8FF00020) and data
    +0x24E40   reloc table: nRelocs=0x13F + 319 reloc entries
    +0x25340   end

Run:   python tools/_decompress_fragment78.py
Output: tools/_fragment78_decompressed.bin  (152384 bytes)

Verified MD5 of source ROM: ed1378bc12115f71209a77844965ba50 (US v1.0).
"""

import hashlib
import os
import struct
import sys

ROM_PATH      = "baserom.z64"
ROM_MD5       = "ed1378bc12115f71209a77844965ba50"
PERS_OFF      = 0x9E93F0   # PERS-SZP wrapper start
YAY0_OFF      = 0x9E9408   # Yay0 stream start (= PERS_OFF + 0x18)
EXPECT_DECOMP = 0x25340    # canonical fragment78 size
OUT_PATH      = "tools/_fragment78_decompressed.bin"


def yay0_decompress(data: bytes) -> bytes:
    """Yay0 (Nintendo LZ-style) decoder. Matches Stadium's runtime
    Yay0_Decompress sufficiently for fragment78."""
    assert data[:4] == b"Yay0", f"bad magic {data[:4]!r}"
    decomp_size    = int.from_bytes(data[4:8],   "big")
    link_table_off = int.from_bytes(data[8:12],  "big")
    data_off       = int.from_bytes(data[12:16], "big")

    out      = bytearray(decomp_size)
    out_pos  = 0
    cmd_pos  = 16
    link_pos = link_table_off
    data_pos = data_off
    cmd      = 0
    cmd_bits = 0

    while out_pos < decomp_size:
        if cmd_bits == 0:
            cmd = int.from_bytes(data[cmd_pos:cmd_pos + 4], "big")
            cmd_pos += 4
            cmd_bits = 32
        if cmd & 0x80000000:
            out[out_pos] = data[data_pos]
            out_pos  += 1
            data_pos += 1
        else:
            link = int.from_bytes(data[link_pos:link_pos + 2], "big")
            link_pos += 2
            offset = link & 0x0FFF
            count  = link >> 12
            if count == 0:
                count = data[data_pos] + 0x12
                data_pos += 1
            else:
                count += 2
            ref = out_pos - offset - 1
            for _ in range(count):
                out[out_pos] = out[ref]
                out_pos += 1
                ref     += 1
        cmd <<= 1
        cmd_bits -= 1

    return bytes(out)


def main() -> int:
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    rom_path = os.path.join(project_root, ROM_PATH)
    out_path = os.path.join(project_root, OUT_PATH)

    with open(rom_path, "rb") as f:
        rom = f.read()
    digest = hashlib.md5(rom).hexdigest()
    if digest != ROM_MD5:
        sys.stderr.write(
            f"ERROR: ROM md5 mismatch (got {digest}, want {ROM_MD5}). "
            f"Stadium NTSC v1.0 required.\n"
        )
        return 1

    if rom[PERS_OFF:PERS_OFF + 8] != b"PERS-SZP":
        sys.stderr.write(f"ERROR: no PERS-SZP magic at ROM 0x{PERS_OFF:X}\n")
        return 1

    pers_payload_off = int.from_bytes(rom[PERS_OFF + 0x08:PERS_OFF + 0x0C], "big")
    pers_decomp_a    = int.from_bytes(rom[PERS_OFF + 0x0C:PERS_OFF + 0x10], "big")
    pers_decomp_b    = int.from_bytes(rom[PERS_OFF + 0x10:PERS_OFF + 0x14], "big")
    if pers_payload_off != 0x18 or pers_decomp_a != EXPECT_DECOMP or pers_decomp_b != EXPECT_DECOMP:
        sys.stderr.write(
            f"ERROR: PERS-SZP header values changed: "
            f"payload_off=0x{pers_payload_off:X} decomp_a=0x{pers_decomp_a:X} "
            f"decomp_b=0x{pers_decomp_b:X}\n"
        )
        return 1

    decomp = yay0_decompress(rom[YAY0_OFF:])
    if len(decomp) != EXPECT_DECOMP:
        sys.stderr.write(
            f"ERROR: decomp size 0x{len(decomp):X} != expected 0x{EXPECT_DECOMP:X}\n"
        )
        return 1

    if decomp[8:16] != b"FRAGMENT":
        sys.stderr.write("ERROR: decomp output missing FRAGMENT magic at +0x08\n")
        return 1

    reloc_off = int.from_bytes(decomp[0x14:0x18], "big")
    size_ram  = int.from_bytes(decomp[0x1C:0x20], "big")
    n_relocs  = int.from_bytes(decomp[reloc_off:reloc_off + 4], "big")
    if reloc_off != 0x24E40 or size_ram != 0x24E40 or n_relocs != 0x13F:
        sys.stderr.write(
            f"WARN: layout values shifted: "
            f"relocOff=0x{reloc_off:X} sizeInRam=0x{size_ram:X} "
            f"nRelocs=0x{n_relocs:X}\n"
        )

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(decomp)

    print(f"OK: wrote {out_path} ({len(decomp)} bytes = 0x{len(decomp):X})")
    print(f"  J trampoline:   0x{int.from_bytes(decomp[:4], 'big'):08X}")
    print(f"  relocOffset:    0x{reloc_off:X}")
    print(f"  sizeInRam:      0x{size_ram:X}")
    print(f"  nRelocations:   0x{n_relocs:X} ({n_relocs})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
