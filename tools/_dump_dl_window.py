"""Dump a window of bytes from build/last_run_freeze_dl.bin
disassembled as F3DEX2 cmds."""
import struct, sys

PATH = "build/last_run_freeze_dl.bin"
START = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x300
END   = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x380
DL_BASE = 0x80209500

OP = {
    0x00: "G_NOOP", 0x01: "G_VTX", 0x05: "G_TRI1", 0x06: "G_TRI2",
    0xD7: "G_TEXTURE", 0xD8: "G_POPMTX", 0xD9: "G_GEOMETRYMODE",
    0xDA: "G_MTX", 0xDB: "G_MOVEWORD", 0xDC: "G_MOVEMEM",
    0xDD: "G_LOAD_UCODE", 0xDE: "G_DL", 0xDF: "G_ENDDL",
    0xE2: "G_SETOTHERMODE_L", 0xE3: "G_SETOTHERMODE_H",
    0xE4: "G_TEXRECT", 0xE6: "G_RDPLOADSYNC", 0xE7: "G_RDPPIPESYNC",
    0xE9: "G_RDPFULLSYNC", 0xED: "G_SETSCISSOR",
    0xF1: "G_RDPHALF_2", 0xF2: "G_SETTILESIZE", 0xF3: "G_LOADBLOCK",
    0xF4: "G_LOADTILE", 0xF5: "G_SETTILE", 0xFA: "G_SETPRIMCOLOR",
    0xFB: "G_SETENVCOLOR", 0xFC: "G_SETCOMBINE", 0xFD: "G_SETTIMG",
    0xFE: "G_SETZIMG", 0xFF: "G_SETCIMG",
}

with open(PATH, "rb") as f:
    data = f.read()

for off in range(START, END, 8):
    if off + 8 > len(data):
        break
    w0 = struct.unpack(">I", data[off:off+4])[0]
    w1 = struct.unpack(">I", data[off+4:off+8])[0]
    op = (w0 >> 24) & 0xFF
    name = OP.get(op, f"OP_{op:02X}")
    extra = ""
    if op == 0xDE:  # G_DL
        branch = (w0 >> 16) & 0x01
        extra = f"  branch={branch} target={w1:#010x}"
    elif op == 0xDF:
        extra = "  (ENDDL)"
    print(f"  +0x{off:04x} @{DL_BASE+off:#010x}  op=0x{op:02X} {name:18s} w0={w0:#010x} w1={w1:#010x}{extra}")
