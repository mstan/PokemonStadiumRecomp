"""Dump RDRAM around 0x209E78 — that's the parent DL location where the
last successful G_DL event fired (per interp_cf), with the bad cmd
w0=0xDEFC0E0F w1=0x5AD12C91.

If the bad cmd is intact at this offset, we have ground truth. If not,
Stadium overwrote it after the freeze.

Also: walk backward to find the START of this DL (look for G_ENDDL of a
preceding chunk, or just dump 64 cmds before/after).
"""
import struct
from pathlib import Path

rdram = Path("build/last_run_rdram.bin").read_bytes()

CENTER = 0x1A0390
START = max(0, CENTER - 32 * 8)
END = min(len(rdram), CENTER + 64 * 8)

print(f"RDRAM dump around offset 0x{CENTER:08X} (vaddr 0x{0x80000000+CENTER:08X})")
print(f"  range: [0x{START:08X}..0x{END:08X}]\n")

names = {
    0xDE: "G_DL", 0xDF: "G_ENDDL", 0xDA: "G_BRANCH_Z",
    0xDB: "G_MOVEWORD", 0xDC: "G_MOVEMEM", 0xD9: "G_GEOMETRYMODE",
    0xD7: "G_TEXTURE", 0xE7: "G_RDPPIPESYNC", 0xE6: "G_RDPLOADSYNC",
    0xE5: "G_RDPTILESYNC", 0xE3: "G_SETOTHERMODE_L",
    0xE2: "G_SETOTHERMODE_H", 0xE1: "G_RDPHALF_1", 0xE4: "G_TEXRECT",
    0xF1: "G_RDPHALF_2", 0xF2: "G_SETTILESIZE", 0xF3: "G_LOADBLOCK",
    0xF5: "G_SETTILE", 0xFA: "G_SETPRIMCOLOR", 0xFB: "G_SETENVCOLOR",
    0xFC: "G_SETCOMBINE", 0xFD: "G_SETTIMG", 0xED: "G_SETSCISSOR",
    0x01: "G_VTX", 0x05: "G_TRI1", 0x06: "G_TRI2", 0x00: "G_NOOP",
}

for k in range(START, END, 8):
    if k + 8 > len(rdram):
        break
    a, b = struct.unpack(">II", rdram[k:k+8])
    op = (a >> 24) & 0xFF
    name = names.get(op, "?")
    marker = ""
    if k == CENTER:
        marker = "  <<<<< parent PC at last cf event"
    if op == 0xDE:
        push = (a >> 16) & 0xFF
        if push not in (0, 1):
            marker += f"  ** BAD push=0x{push:02X}"
    print(f"  @0x{k:08X} (vaddr 0x{0x80000000+k:08X}): "
          f"w0=0x{a:08X} w1=0x{b:08X}  op=0x{op:02X} {name}{marker}")
