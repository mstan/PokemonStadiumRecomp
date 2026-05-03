"""Decode F3DEX2 cmds at a given RDRAM offset (from build/last_run_rdram.bin)."""
import struct, sys

PATH = "build/last_run_rdram.bin"
START = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x000eb8
N = int(sys.argv[2]) if len(sys.argv) > 2 else 16

OP = {0x00:"G_NOOP",0x01:"G_VTX",0x05:"G_TRI1",0x06:"G_TRI2",
      0xD7:"G_TEXTURE",0xD8:"G_POPMTX",0xD9:"G_GEOMETRYMODE",
      0xDA:"G_MTX",0xDB:"G_MOVEWORD",0xDC:"G_MOVEMEM",
      0xDD:"G_LOAD_UCODE",0xDE:"G_DL",0xDF:"G_ENDDL",
      0xE2:"G_SETOTHERMODE_L",0xE3:"G_SETOTHERMODE_H",
      0xED:"G_SETSCISSOR",0xFC:"G_SETCOMBINE",0xFD:"G_SETTIMG"}

with open(PATH, "rb") as f:
    data = f.read()

print(f"=== rdram[{START:#08x}..{START+N*8:#08x}] (vaddr 0x80000000+) ===")
for off in range(START, START + N*8, 8):
    if off + 8 > len(data):
        print(f"  +0x{off:08x}  END OF RDRAM")
        break
    w0 = struct.unpack(">I", data[off:off+4])[0]
    w1 = struct.unpack(">I", data[off+4:off+8])[0]
    op = (w0 >> 24) & 0xFF
    name = OP.get(op, f"OP_{op:02X}")
    print(f"  +0x{off:08x} @{0x80000000+off:#010x}  op=0x{op:02X} {name:18s} w0={w0:#010x} w1={w1:#010x}")
