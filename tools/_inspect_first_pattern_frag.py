"""The first wrapper found by the engine's BFS scan of vram 0x8FF00000.
Failure is at vram 0x8FF080B8 (offset 0x80B8) — 'pref' instruction
followed by a string of INVALID instructions.

We want to know:
  1. What's at offset 0x80B8 and around it (what does the data look
     like).
  2. What conditional branch or jtbl reached it (i.e., what's the
     control-flow ENTRY into 0x80B8 — was there a branch with a long
     positive offset somewhere in the function?).
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


with open("F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64", "rb") as f:
    rom = f.read()

# Walk ROM from start finding the first PERS-SZP/Yay0 wrapper
# decompressing to a fragment at vram 0x8FF00000.
first_wrapper = None
for off in range(0, len(rom) - 0x18):
    if rom[off:off + 8] == b"PERS-SZP":
        yay0_off = off + 0x18
        if yay0_off + 16 < len(rom) and rom[yay0_off:yay0_off + 4] == b"Yay0":
            body = yay0_decompress(rom[yay0_off:yay0_off + 0x100000])
            if body is None:
                continue
            if (len(body) >= 0x10 and
                body[0:4] == b"\x0B\xFC\x00\x08" and
                body[8:16] == b"FRAGMENT"):
                first_wrapper = (off, body)
                break

if first_wrapper is None:
    print("no fragment78-shaped wrapper found")
    raise SystemExit(1)

wrap_off, body = first_wrapper
print(f"first fragment78-shaped wrapper at ROM 0x{wrap_off:X}")
print(f"  decomp size: 0x{len(body):X}")
print(f"  relocOffset: 0x{int.from_bytes(body[0x14:0x18], 'big'):X}")
print(f"  sizeInRam:   0x{int.from_bytes(body[0x1C:0x20], 'big'):X}")
print()

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
    if op == 0x04:
        target = vram + 4 + (((imm ^ 0x8000) - 0x8000) << 2)
        return f"beq {regs[rs]}, {regs[rt]}, 0x{target:08X}"
    if op == 0x05:
        target = vram + 4 + (((imm ^ 0x8000) - 0x8000) << 2)
        return f"bne {regs[rs]}, {regs[rt]}, 0x{target:08X}"
    if op == 0x06:
        target = vram + 4 + (((imm ^ 0x8000) - 0x8000) << 2)
        return f"blez {regs[rs]}, 0x{target:08X}"
    if op == 0x07:
        target = vram + 4 + (((imm ^ 0x8000) - 0x8000) << 2)
        return f"bgtz {regs[rs]}, 0x{target:08X}"
    if op == 0x09: return f"addiu {regs[rt]}, {regs[rs]}, 0x{imm:04X}"
    if op == 0x0A: return f"slti  {regs[rt]}, {regs[rs]}, 0x{imm:04X}"
    if op == 0x0F: return f"lui {regs[rt]}, 0x{imm:04X}"
    if op == 0x23: return f"lw {regs[rt]}, 0x{imm:04X}({regs[rs]})"
    if op == 0x2B: return f"sw {regs[rt]}, 0x{imm:04X}({regs[rs]})"
    return f"OP=0x{op:02X} word=0x{w:08X}"


print("Bytes around offset 0x80B8 (where 'pref' was emitted):")
for off in range(0x80A0, 0x80F0, 4):
    if off + 4 > len(body): break
    word = int.from_bytes(body[off:off + 4], "big")
    vram = 0x8FF00000 + off
    print(f"  +0x{off:05X} (vram 0x{vram:08X}): 0x{word:08X}  {decode(word, vram)}")

print()
print("Searching for ANY conditional branch within [+0x20, 0x80B8) whose ")
print("target is at or past 0x80B8...")
hits = 0
for off in range(0x20, 0x80B8, 4):
    word = int.from_bytes(body[off:off + 4], "big")
    op = (word >> 26) & 0x3F
    imm = word & 0xFFFF
    if op in (0x01, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17):
        signed_imm = ((imm ^ 0x8000) - 0x8000)
        target = (0x8FF00000 + off) + 4 + (signed_imm << 2)
        target_off = target - 0x8FF00000
        if target_off >= 0x80B8:
            print(f"  +0x{off:05X}: 0x{word:08X} -> target 0x{target:08X} (off 0x{target_off:X})")
            hits += 1
            if hits >= 5: break
print(f"  ({hits} hits)")

print()
print("Also looking for JR-via-jtbl with table that could include 0x8FF080B8...")
# A jr $reg with a jtbl whose entries include offset 0x80B8 would
# be the other path. Look for jr <reg> earlier in the function and
# inspect potential jtbl bases.
for off in range(0x20, 0x80B8, 4):
    word = int.from_bytes(body[off:off + 4], "big")
    if (word >> 26) == 0 and (word & 0x3F) == 0x08:
        rs = (word >> 21) & 0x1F
        if rs != 31:
            print(f"  jr {regs[rs]} at +0x{off:05X}")

print()
print("=== Bytes at 0x70..0x200 (post-arms code area) ===")
for off in range(0x70, 0x200, 4):
    if off + 4 > len(body): break
    word = int.from_bytes(body[off:off + 4], "big")
    vram = 0x8FF00000 + off
    print(f"  +0x{off:05X} (vram 0x{vram:08X}): 0x{word:08X}  {decode(word, vram)}")
