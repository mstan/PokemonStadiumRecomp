"""Decompress wrapper at ROM 0x56E900 and dump the bytes around:
  - the indirect jr at vram 0x8FF00040 (offset 0x40 in body),
  - the jump table at vram 0x8FF0C300 (offset 0xC300).

The pattern run reported `Failed to determine size of jump table at
0x8FF0C300 for instruction at 0x8FF00040` — analyze_function detected
the jr's jtbl, started reading entries, and the FIRST entry pointed
outside the function bounds it computed. We want to understand what
range the entries actually cover and why analyze_function disagrees
with discover_function_bounds.
"""

def yay0_decompress(data):
    if data[:4] != b"Yay0":
        return None
    decomp_size = int.from_bytes(data[4:8], "big")
    link_off = int.from_bytes(data[8:12], "big")
    data_off = int.from_bytes(data[12:16], "big")
    out = bytearray(decomp_size + 32)
    out_pos = 0
    cmd_pos = 16
    link_pos = link_off
    data_pos = data_off
    cmd = 0
    cmd_bits = 0
    try:
        while out_pos < decomp_size:
            if cmd_bits == 0:
                cmd = int.from_bytes(data[cmd_pos:cmd_pos + 4], "big")
                cmd_pos += 4
                cmd_bits = 32
            if cmd & 0x80000000:
                out[out_pos] = data[data_pos]
                out_pos += 1
                data_pos += 1
            else:
                link = int.from_bytes(data[link_pos:link_pos + 2], "big")
                link_pos += 2
                offset = link & 0x0FFF
                count = link >> 12
                if count == 0:
                    count = data[data_pos] + 0x12
                    data_pos += 1
                else:
                    count += 2
                ref = out_pos - offset - 1
                for _ in range(count):
                    if out_pos >= len(out):
                        return None
                    out[out_pos] = out[ref]
                    out_pos += 1
                    ref += 1
            cmd <<= 1
            cmd_bits -= 1
    except IndexError:
        return None
    return bytes(out[:decomp_size])


with open("F:/Projects/PokemonStadiumRecomp/baserom.z64", "rb") as f:
    rom = f.read()

# Pattern run reported __rom_FE56E920. The 0xFE prefix is engine
# synthetic; real wrapper offset is 0x56E920. PERS-SZP at -0x18.
WRAPPER = 0x56E920 - 0x18  # 0x56E908
print(f"Trying wrapper at 0x{WRAPPER:X}")
if rom[WRAPPER : WRAPPER + 8] == b"PERS-SZP":
    print(f"  PERS-SZP magic confirmed at 0x{WRAPPER:X}")
    yay0_off = WRAPPER + 0x18
else:
    print(f"  No PERS-SZP at 0x{WRAPPER:X} - searching nearby")
    for delta in range(-0x100, 0x100, 4):
        off = (0x56E920 + delta) - 0x18
        if off >= 0 and rom[off : off + 8] == b"PERS-SZP":
            print(f"  Found PERS-SZP at 0x{off:X} (delta={delta})")
            WRAPPER = off
            yay0_off = WRAPPER + 0x18
            break

body = yay0_decompress(rom[yay0_off : yay0_off + 0x100000])
if body is None:
    print("  Decompress FAILED")
    raise SystemExit(1)

print(f"body len 0x{len(body):X}")
print()
print("First 0x60 bytes of body (header + entry+0x20 area):")
regs = ["$0","$1","$v0","$v1","$a0","$a1","$a2","$a3",
        "$t0","$t1","$t2","$t3","$t4","$t5","$t6","$t7",
        "$s0","$s1","$s2","$s3","$s4","$s5","$s6","$s7",
        "$t8","$t9","$k0","$k1","$gp","$sp","$fp","$ra"]


def decode(w, vram):
    op = (w >> 26) & 0x3F
    rs = (w >> 21) & 0x1F
    rt = (w >> 16) & 0x1F
    rd = (w >> 11) & 0x1F
    sa = (w >> 6) & 0x1F
    fn = w & 0x3F
    imm = w & 0xFFFF
    target26 = w & 0x03FFFFFF
    if op == 0:
        if fn == 0x08: return f"jr {regs[rs]}"
        if fn == 0x09: return f"jalr {regs[rd]}, {regs[rs]}"
        if fn == 0x21: return f"addu {regs[rd]}, {regs[rs]}, {regs[rt]}"
        if fn == 0x25: return f"or {regs[rd]}, {regs[rs]}, {regs[rt]}"
        if fn == 0x00: return f"sll {regs[rd]}, {regs[rt]}, {sa}"
        if fn == 0x02: return f"srl {regs[rd]}, {regs[rt]}, {sa}"
        if fn == 0x03: return f"sra {regs[rd]}, {regs[rt]}, {sa}"
        return f"SPECIAL fn=0x{fn:02X}"
    if op == 0x02: return f"j 0x{(target26 << 2) | (vram & 0xF0000000):08X}"
    if op == 0x03: return f"jal 0x{(target26 << 2) | (vram & 0xF0000000):08X}"
    if op == 0x04: return f"beq {regs[rs]}, {regs[rt]}, +0x{imm:04X}"
    if op == 0x05: return f"bne {regs[rs]}, {regs[rt]}, +0x{imm:04X}"
    if op == 0x09: return f"addiu {regs[rt]}, {regs[rs]}, 0x{imm:04X}"
    if op == 0x0A: return f"slti  {regs[rt]}, {regs[rs]}, 0x{imm:04X}"
    if op == 0x0B: return f"sltiu {regs[rt]}, {regs[rs]}, 0x{imm:04X}"
    if op == 0x0F: return f"lui {regs[rt]}, 0x{imm:04X}"
    if op == 0x23: return f"lw {regs[rt]}, 0x{imm:04X}({regs[rs]})"
    if op == 0x2B: return f"sw {regs[rt]}, 0x{imm:04X}({regs[rs]})"
    return f"OP=0x{op:02X} word=0x{w:08X}"


for off in range(0, 0x60, 4):
    word = int.from_bytes(body[off:off + 4], "big")
    vram = 0x8FF00000 + off
    print(f"  +0x{off:05X} (vram 0x{vram:08X}): 0x{word:08X}  {decode(word, vram)}")

print()
print("Bytes around the jump-table at vram 0x8FF0C300 (offset 0xC300):")
for off in range(0xC2F0, 0xC340, 4):
    if off + 4 > len(body):
        break
    word = int.from_bytes(body[off:off + 4], "big")
    print(f"  +0x{off:05X}: 0x{word:08X}")

print()
print("Function header reports:")
print(f"  +0x14 relocOffset: 0x{int.from_bytes(body[0x14:0x18], 'big'):X}")
print(f"  +0x1C sizeInRam:   0x{int.from_bytes(body[0x1C:0x20], 'big'):X}")
