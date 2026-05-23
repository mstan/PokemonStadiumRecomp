#!/usr/bin/env python3
"""Scan fragment62's body bytes for any 4-byte aligned 0x8FF0XXXX
literals, regardless of whether they're in the reloc table. The
build-time R_MIPS_32 census found ZERO pattern-bucket relocs in
fragment62 — meaning either the runtime-observed 0x8FF0ABFC literal
isn't actually from fragment62, OR it's a raw .word constant with
no reloc entry."""

import struct
import re

ROM = "F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64"
FRAG62_ROM = 0x2EA8C0
FRAG62_END = 0x378D00
FRAG62_VRAM = 0x84300000

with open(ROM, "rb") as f:
    rom = f.read()
body = rom[FRAG62_ROM:FRAG62_END]

# Direct byte search for 0x8FF0ABFC
hits_target = [m.start() for m in re.finditer(b"\x8f\xf0\xab\xfc", body)]
print(f"raw 8FF0ABFC byte hits in fragment62: {len(hits_target)}")
for h in hits_target[:20]:
    print(f"  body+0x{h:05X}  vram+0x{FRAG62_VRAM+h:08X}")
print()

# All aligned 0x8FF0XXXX literals
hits2 = []
for i in range(0, len(body) - 4, 4):
    w = struct.unpack(">I", body[i:i+4])[0]
    if (w & 0xFFF00000) == 0x8FF00000:
        hits2.append((i, w))
print(f"aligned 0x8FF0XXXX literals: {len(hits2)}")
for h, w in hits2[:30]:
    print(f"  body+0x{h:05X}  literal=0x{w:08X}  target_offset=0x{w & 0xFFFFF:05X}")
print()

# Also check ALL fragments by scanning the whole ROM aligned
print("scanning full ROM for aligned 8FF0ABFC...")
n = 0
locations = []
for i in range(0, len(rom) - 4, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    if w == 0x8FF0ABFC:
        n += 1
        locations.append(i)
        if len(locations) <= 20:
            print(f"  rom+0x{i:08X}")
print(f"total ROM-wide 4-aligned 8FF0ABFC: {n}")
