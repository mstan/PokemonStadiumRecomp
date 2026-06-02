"""Dump the full failing gfx DL buffer (0x80191FC0..0x801A1FD0, ~64KB)
from live rdram and decode it. Find the bad G_DL command and surrounding
context so we can reverse-engineer which Stadium function emitted it.
"""
import json, socket, sys

ADDR = ("127.0.0.1", 4371)


def peek(vaddr, n):
    s = socket.create_connection(ADDR, timeout=10)
    s.sendall((json.dumps({"cmd": "rdram_peek", "addr": vaddr, "n": n}) + "\n").encode())
    buf = b""
    while True:
        c = s.recv(65536)
        if not c: break
        buf += c
        if buf.endswith(b"\n"): break
    s.close()
    return json.loads(buf.decode())


DL_START = 0x80191FC0
DL_END = 0x801A1FD0
DL_SIZE = DL_END - DL_START
CHUNK = 256

print(f"Dumping {DL_SIZE} bytes from 0x{DL_START:08X}..0x{DL_END:08X}")
data = bytearray()
for off in range(0, DL_SIZE, CHUNK):
    n = min(CHUNK, DL_SIZE - off)
    r = peek(DL_START + off, n)
    if not r.get("ok"):
        print(f"  read failed at offset 0x{off:X}: {r}")
        break
    h = r["hex"]
    data.extend(bytes.fromhex(h))
    if off % 0x1000 == 0:
        sys.stdout.write(f"  0x{off:05X}/0x{DL_SIZE:05X}\r")
        sys.stdout.flush()
print()

# Save raw
out_path = "build/failing_dl.bin"
with open(out_path, "wb") as f:
    f.write(data)
print(f"saved {len(data)} bytes to {out_path}")
print()

# Decode F3DEX2 cmd-by-cmd
op_names = {
    0x00: "G_NOOP", 0x01: "G_VTX", 0x03: "G_TRI", 0x05: "G_TRI1", 0x06: "G_TRI2",
    0xD7: "G_TEXTURE", 0xD8: "G_POPMTX", 0xD9: "G_GEOMETRYMODE",
    0xDA: "G_MTX", 0xDB: "G_MOVEWORD", 0xDC: "G_MOVEMEM",
    0xDD: "G_LOAD_UCODE", 0xDE: "G_DL", 0xDF: "G_ENDDL",
    0xE0: "G_SPNOOP", 0xE1: "G_RDPHALF_1", 0xE2: "G_SETOTHERMODE_L",
    0xE3: "G_SETOTHERMODE_H", 0xE4: "G_TEXRECT", 0xE5: "G_TEXRECTFLIP",
    0xE6: "G_RDPLOADSYNC", 0xE7: "G_RDPPIPESYNC", 0xE8: "G_RDPTILESYNC",
    0xE9: "G_RDPFULLSYNC",
    0xEA: "G_SETKEYGB", 0xEB: "G_SETKEYR", 0xEC: "G_SETCONVERT",
    0xED: "G_SETSCISSOR", 0xEE: "G_SETPRIMDEPTH", 0xEF: "G_RDPSETOTHERMODE",
    0xF0: "G_LOADTLUT", 0xF2: "G_SETTILESIZE", 0xF3: "G_LOADBLOCK",
    0xF4: "G_LOADTILE", 0xF5: "G_SETTILE", 0xF6: "G_FILLRECT",
    0xF7: "G_SETFILLCOLOR", 0xF8: "G_SETFOGCOLOR", 0xF9: "G_SETBLENDCOLOR",
    0xFA: "G_SETPRIMCOLOR", 0xFB: "G_SETENVCOLOR", 0xFC: "G_SETCOMBINE",
    0xFD: "G_SETTIMG", 0xFE: "G_SETZIMG", 0xFF: "G_SETCIMG",
}

# Find the bad G_DL with target = 0x80208F20
import struct
bad_off = -1
for off in range(0, len(data) - 8, 8):
    w0, w1 = struct.unpack(">II", data[off:off+8])
    if w0 == 0xDE010000 and w1 == 0x80208F20:
        bad_off = off
        break

if bad_off < 0:
    print("ERROR: bad G_DL not found in dump")
    raise SystemExit(1)

bad_vaddr = DL_START + bad_off
print(f"Bad G_DL at file offset 0x{bad_off:X} (vaddr 0x{bad_vaddr:08X})")
print()

# Show context: 32 cmds before and 8 cmds after
def fmt_cmd(off, w0, w1):
    op = (w0 >> 24) & 0xFF
    name = op_names.get(op, f"0x{op:02X}")
    extra = ""
    if op == 0xDE:
        extra = f"  push={(w0 >> 16) & 0xFF:#04x} target=0x{w1:08X}"
        if 0x80200000 <= w1 < 0x80300000:
            extra += "  <-- AUDIO RANGE"
    elif op == 0xFB:
        extra = f"  RGBA=0x{w1:08X}"
    elif op == 0xDB:
        extra = f"  index={(w0 >> 8) & 0xFF:#04x} offset=0x{w0 & 0xFF:02X} value=0x{w1:08X}"
    elif op == 0x01:
        n_v = ((w0 >> 12) & 0xFF) + 1
        extra = f"  n_verts={n_v}"
    elif op == 0xE3:
        # SetOtherMode_H
        shift = (w0 >> 8) & 0xFF
        length = (w0 & 0xFF) + 1
        extra = f"  shift={shift} len={length}"
    return f"  +0x{off:05X} (vaddr 0x{DL_START + off:08X}): {w0:08X} {w1:08X}  {name:<20}{extra}"

print(f"=== context: 32 cmds before bad G_DL ===")
start = max(0, bad_off - 32 * 8)
for off in range(start, bad_off + 8 * 8, 8):
    if off >= len(data) - 8:
        break
    w0, w1 = struct.unpack(">II", data[off:off+8])
    marker = "  <-- BAD" if off == bad_off else ""
    print(fmt_cmd(off, w0, w1) + marker)

# Also count: how many G_DL pushes precede the bad one? What targets?
print(f"\n=== ALL G_DL push commands in this DL (in walk order) ===")
push_count = 0
for off in range(0, len(data) - 8, 8):
    w0, w1 = struct.unpack(">II", data[off:off+8])
    if (w0 >> 24) == 0xDE:
        push = (w0 >> 16) & 0xFF
        marker = " <-- BAD" if off == bad_off else ""
        print(f"  +0x{off:05X}: push={push:#04x} target=0x{w1:08X}{marker}")
        push_count += 1
print(f"\ntotal G_DL commands: {push_count}")
