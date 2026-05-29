"""Hexdump fragment 31's header + unk_table from ROM."""
import struct
with open("F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64", "rb") as f:
    f.seek(0x2552C0)
    data = f.read(0x300)

print("=== fragment31 ROM 0x2552C0..0x2555C0 (u32 BE) ===")
print("    offset    word0    word1    word2    word3   |  ascii")
for off in range(0, len(data), 16):
    words = struct.unpack(">4I", data[off:off+16])
    asc = "".join(chr(b) if 32 <= b < 127 else "." for b in data[off:off+16])
    print(f"  +0x{off:03x}  {words[0]:08x} {words[1]:08x} {words[2]:08x} {words[3]:08x}  | {asc}")

# Highlight offset 0x1D0 (the lookup-miss target).
print()
print("Word at offset 0x1D0:")
val = struct.unpack(">I", data[0x1D0:0x1D4])[0]
print(f"  0x{val:08x}  (decimal {val})")

# If unk_table is u32 pointers from offset 0x20..0x200, every 4 bytes is one pointer.
print()
print("=== if unk_table = u32 pointers, 0x20..0x200 = 120 entries ===")
import struct
for i, off in enumerate(range(0x20, 0x200, 4)):
    v = struct.unpack(">I", data[off:off+4])[0]
    if i < 8 or v != 0:
        flag = ""
        if 0x81000000 <= v < 0x81100000:
            flag = " <- inside fragment31's link-time vram"
        elif 0x80000000 <= v < 0x82000000:
            flag = " <- looks like a vram pointer"
        print(f"  unk_table[{i:3}] @+0x{off:03x} = 0x{v:08x}{flag}")
        if i > 80: break
