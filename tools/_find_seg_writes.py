"""Find all G_MOVEWORD G_MW_SEGMENT writes in the captured DL.
G_MOVEWORD opcode 0xDB. F3DEX2: w0=(0xDB<<24)|(index<<16)|offset, w1=data.
G_MW_SEGMENT index = 0x06; offset = (segment_id << 2)."""
import struct

PATH = "build/last_run_freeze_dl.bin"
DL_BASE = 0x80209500

with open(PATH, "rb") as f:
    data = f.read()

print(f"=== G_MOVEWORD G_MW_SEGMENT writes in {PATH} ===")
for off in range(0, len(data) - 8, 8):
    w0 = struct.unpack(">I", data[off:off+4])[0]
    w1 = struct.unpack(">I", data[off+4:off+8])[0]
    op = (w0 >> 24) & 0xFF
    if op != 0xDB:
        continue
    idx = (w0 >> 16) & 0xFF
    offset = w0 & 0xFFFF
    if idx != 0x06:
        continue
    seg_id = (offset >> 2) & 0x0F
    print(f"  +0x{off:04x} @{DL_BASE+off:#010x}  G_MW_SEGMENT seg{seg_id} = {w1:#010x}")

# Also list all G_MOVEWORD entries by index
print()
print(f"=== ALL G_MOVEWORD writes ===")
mw_seen = {}
for off in range(0, len(data) - 8, 8):
    w0 = struct.unpack(">I", data[off:off+4])[0]
    w1 = struct.unpack(">I", data[off+4:off+8])[0]
    op = (w0 >> 24) & 0xFF
    if op != 0xDB:
        continue
    idx = (w0 >> 16) & 0xFF
    mw_seen[idx] = mw_seen.get(idx, 0) + 1
for idx, ct in sorted(mw_seen.items()):
    name = {0x00: "G_MW_MATRIX", 0x02: "G_MW_NUMLIGHT", 0x04: "G_MW_CLIP",
            0x06: "G_MW_SEGMENT", 0x08: "G_MW_FOG", 0x0A: "G_MW_LIGHTCOL",
            0x0E: "G_MW_PERSPNORM"}.get(idx, f"idx{idx:#x}")
    print(f"  {name:18s} = {ct} writes")
