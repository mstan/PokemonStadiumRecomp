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

body = yay0_decompress(rom[0x8CC418 : 0x8CC418 + 0x100000])
print(f"decomp len: 0x{len(body):X}")
print()
print("First 0x40 bytes:")
for i in range(0, 0x40, 4):
    w = int.from_bytes(body[i:i + 4], "big")
    chars = "".join(chr(b) if 32 <= b < 127 else "." for b in body[i:i + 4])
    print(f"  +0x{i:02X}: 0x{w:08X}  \"{chars}\"")

print()
print("Code-region scan: count of likely-instructions in [0x20, 0x9000)")
# Heuristic: count words with op==0 (SPECIAL) or known opcodes (lui=0x0F,
# addiu=0x09, lw=0x23, sw=0x2B, beq=0x04, bne=0x05, j=0x02, jal=0x03,
# regimm=0x01) — gives a sense of where real code is.
in_code = 0
in_data = 0
for off in range(0x20, 0x9000, 4):
    w = int.from_bytes(body[off:off + 4], "big")
    op = (w >> 26) & 0x3F
    if op in (0, 1, 2, 3, 4, 5, 9, 0x0F, 0x23, 0x2B, 0x0E, 0x0C, 0x0D, 0x14, 0x15):
        in_code += 1
    else:
        in_data += 1
print(f"  code-like: {in_code}")
print(f"  data-like: {in_data}")

# Also dump all bytes that decode to jr $ra-style returns
# (jr can be jr $rs where rs in 1..31, encoded as op=0/fn=8)
print()
print("All jr/jalr instructions in body:")
for off in range(0x20, 0x9000, 4):
    w = int.from_bytes(body[off:off + 4], "big")
    op = (w >> 26) & 0x3F
    fn = w & 0x3F
    rs = (w >> 21) & 0x1F
    if op == 0 and fn in (0x08, 0x09):
        print(f"  +0x{off:05X} jr/jalr from $r{rs} (word=0x{w:08X})")
