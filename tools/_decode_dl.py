"""Decode an F3DEX2 DL bin from disk and surface suspicious commands.

Each command is two big-endian u32s (w0, w1) = 8 bytes total. The
hung-DL bin is captured in BE byte order (post_mortem dumps via XOR-3
endian-corrected byte reads so the on-disk bytes match the N64 view).

Suspicious patterns we flag:
- G_DL with branch=1 to a low/zero/garbage address — infinite loop bait
- G_DL where target == this DL's start address — direct self-loop
- G_LOAD_UCODE
- G_ENDDL appearing in the middle of the buffer
- Unknown opcodes
"""
import struct, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_freeze_dl.bin"
DL_START = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x80209500

OP = {
    0x00: "G_NOOP",
    0x01: "G_VTX",
    0x02: "G_MODIFYVTX",
    0x03: "G_CULLDL",
    0x04: "G_BRANCH_Z",
    0x05: "G_TRI1",
    0x06: "G_TRI2",
    0x07: "G_QUAD",
    0x08: "G_LINE3D",
    0xD3: "G_SPECIAL_3",
    0xD4: "G_SPECIAL_2",
    0xD5: "G_SPECIAL_1",
    0xD6: "G_DMA_IO",
    0xD7: "G_TEXTURE",
    0xD8: "G_POPMTX",
    0xD9: "G_GEOMETRYMODE",
    0xDA: "G_MTX",
    0xDB: "G_MOVEWORD",
    0xDC: "G_MOVEMEM",
    0xDD: "G_LOAD_UCODE",
    0xDE: "G_DL",
    0xDF: "G_ENDDL",
    0xE0: "G_SPNOOP",
    0xE1: "G_RDPHALF_1",
    0xE2: "G_SETOTHERMODE_L",
    0xE3: "G_SETOTHERMODE_H",
    0xE4: "G_TEXRECT",
    0xE5: "G_TEXRECTFLIP",
    0xE6: "G_RDPLOADSYNC",
    0xE7: "G_RDPPIPESYNC",
    0xE8: "G_RDPTILESYNC",
    0xE9: "G_RDPFULLSYNC",
    0xEA: "G_SETKEYGB",
    0xEB: "G_SETKEYR",
    0xEC: "G_SETCONVERT",
    0xED: "G_SETSCISSOR",
    0xEE: "G_SETPRIMDEPTH",
    0xEF: "G_RDPSETOTHERMODE",
    0xF0: "G_LOADTLUT",
    0xF1: "G_RDPHALF_2",
    0xF2: "G_SETTILESIZE",
    0xF3: "G_LOADBLOCK",
    0xF4: "G_LOADTILE",
    0xF5: "G_SETTILE",
    0xF6: "G_FILLRECT",
    0xF7: "G_SETFILLCOLOR",
    0xF8: "G_SETFOGCOLOR",
    0xF9: "G_SETBLENDCOLOR",
    0xFA: "G_SETPRIMCOLOR",
    0xFB: "G_SETENVCOLOR",
    0xFC: "G_SETCOMBINE",
    0xFD: "G_SETTIMG",
    0xFE: "G_SETZIMG",
    0xFF: "G_SETCIMG",
}

with open(PATH, "rb") as f:
    data = f.read()
ncmds = len(data) // 8
print(f"file={PATH}  bytes={len(data)}  cmds={ncmds}  dl_start={DL_START:#010x}")
print()

# Pass 1: tally opcodes
counts = {}
for i in range(ncmds):
    w0 = struct.unpack(">I", data[i*8:i*8+4])[0]
    op = (w0 >> 24) & 0xFF
    counts[op] = counts.get(op, 0) + 1
print("=== opcode histogram (descending) ===")
for op, n in sorted(counts.items(), key=lambda x: -x[1]):
    name = OP.get(op, "?")
    print(f"  0x{op:02X} {name:20s} {n}")
print()

# Pass 2: flag suspicious commands
print("=== suspicious commands ===")
flagged = 0
for i in range(ncmds):
    w0 = struct.unpack(">I", data[i*8:i*8+4])[0]
    w1 = struct.unpack(">I", data[i*8+4:i*8+8])[0]
    op = (w0 >> 24) & 0xFF
    name = OP.get(op, f"UNK_{op:02X}")
    addr = DL_START + i*8

    if op == 0xDE:  # G_DL
        branch = (w0 >> 16) & 0xFF
        target = w1
        rel = target - DL_START
        suspicious = False
        why = []
        if branch == 1:
            why.append("BRANCH=1")
        if target == DL_START:
            why.append("TARGET=DL_START (SELF)")
            suspicious = True
        if 0 <= rel < len(data):
            why.append(f"TARGET=DL+{rel:#x} (in this DL!)")
            suspicious = True
        if target < 0x80000000 or target >= 0x80800000:
            why.append(f"TARGET={target:#010x} (out of RDRAM)")
            suspicious = True
        if branch == 1 or suspicious:
            print(f"  +{i*8:#06x} @{addr:#010x}  {name:14s} branch={branch} target={target:#010x}  [{','.join(why)}]")
            flagged += 1
    elif op == 0xDD:  # G_LOAD_UCODE
        print(f"  +{i*8:#06x} @{addr:#010x}  {name:14s} w0={w0:#010x} w1={w1:#010x}  [LOAD_UCODE]")
        flagged += 1
    elif op == 0xDF and i < ncmds - 1:  # G_ENDDL not at end
        print(f"  +{i*8:#06x} @{addr:#010x}  {name:14s}  [G_ENDDL mid-buffer; will pop return]")
        flagged += 1
    elif op not in OP:
        print(f"  +{i*8:#06x} @{addr:#010x}  UNKNOWN_OP 0x{op:02X}  w0={w0:#010x} w1={w1:#010x}")
        flagged += 1

print(f"\n  flagged {flagged} commands")

# Tail: last 20 commands (where the hang likely is)
print()
print("=== last 20 commands ===")
start = max(0, ncmds - 20)
for i in range(start, ncmds):
    w0 = struct.unpack(">I", data[i*8:i*8+4])[0]
    w1 = struct.unpack(">I", data[i*8+4:i*8+8])[0]
    op = (w0 >> 24) & 0xFF
    name = OP.get(op, f"UNK_{op:02X}")
    print(f"  +{i*8:#06x} {name:20s} w0={w0:#010x} w1={w1:#010x}")
