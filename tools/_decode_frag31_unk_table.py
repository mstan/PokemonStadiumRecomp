"""Decode fragment31's unk_table J-trampoline entries.
unk_table starts at vram 0x81000020, offset 0x10 in the .bin = vram 0x81000030."""
import struct
data = open("disasm/assets/us/fragments/31/fragment31_unk_table.textbin.bin", "rb").read()
for off in range(0, 0x40, 8):
    w0 = struct.unpack(">I", data[off:off+4])[0]
    w1 = struct.unpack(">I", data[off+4:off+8])[0]
    vram = 0x81000020 + off
    op = (w0 >> 26) & 0x3F
    target_vram = (w0 & 0x03FFFFFF) << 2
    note = ""
    if op == 0x02:  # J
        note = f"  J 0x{target_vram:08X}"
    print(f"  +0x{off:04x} vram=0x{vram:08X}  w0={w0:#010x} w1={w1:#010x}{note}")
