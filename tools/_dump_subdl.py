"""Dump raw RDRAM around the CALLed sub-DL targets so we can see what's there.

If there's no G_ENDDL within 1024 cmds, either:
  a) Stadium overwrote the buffer after the freeze
  b) The address isn't a DL at all (data, vertex buffer, etc.)
  c) The buffer is intact but the DL is mostly G_DL BRANCHes that we're not
     following — so G_ENDDL exists in a downstream chain
"""
import struct
import sys
from pathlib import Path

rdram = Path("build/last_run_rdram.bin").read_bytes()

# CALL targets from the freeze DL.
TARGETS = [
    0x80192260, 0x80192670, 0x801926E0, 0x801927F8, 0x80192F10,
    0x80192F98, 0x801936B0, 0x80193740, 0x80193E58, 0x80193EE8,
    0x80194600, 0x801925D0, 0x80194690, 0x80194770,
]

# Also dump a few BRANCH targets (push=0) to compare.
BRANCH_SAMPLES = [0x80215AF0, 0x80217198, 0x80068C68]

for vaddr in TARGETS[:3] + BRANCH_SAMPLES:
    off = vaddr - 0x80000000
    print(f"\n=== vaddr=0x{vaddr:08X}  rdram_off=0x{off:08X}  (first 24 cmds) ===")
    for i in range(24):
        k = off + i * 8
        if k + 8 > len(rdram):
            break
        a, b = struct.unpack(">II", rdram[k:k+8])
        op = (a >> 24) & 0xFF
        # Decode op name for common F3DEX2 ones.
        names = {
            0xDE: "G_DL", 0xDF: "G_ENDDL", 0xDA: "G_BRANCH_Z",
            0xDB: "G_MOVEWORD", 0xDC: "G_MOVEMEM", 0xD9: "G_GEOMETRYMODE",
            0xD7: "G_TEXTURE", 0xE7: "G_RDPPIPESYNC", 0xE6: "G_RDPLOADSYNC",
            0xE5: "G_RDPTILESYNC", 0xE3: "G_SETOTHERMODE_L",
            0xE2: "G_SETOTHERMODE_H", 0xE1: "G_RDPHALF_1", 0xE4: "G_TEXRECT",
            0xF1: "G_RDPHALF_2", 0xF2: "G_SETTILESIZE", 0xF3: "G_LOADBLOCK",
            0xF5: "G_SETTILE", 0xFA: "G_SETPRIMCOLOR", 0xFB: "G_SETENVCOLOR",
            0xFC: "G_SETCOMBINE", 0xFD: "G_SETTIMG", 0xED: "G_SETSCISSOR",
            0x01: "G_VTX", 0x05: "G_TRI1", 0x06: "G_TRI2",
        }
        name = names.get(op, "?")
        print(f"    +0x{i*8:03X}  w0=0x{a:08X} w1=0x{b:08X}  op=0x{op:02X} {name}")
