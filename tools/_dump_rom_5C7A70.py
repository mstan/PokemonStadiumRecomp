"""Dump ROM bytes at 0x5C7A70 — the source of id=0xC0 fragment."""
import struct

with open('F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64', 'rb') as f:
    f.seek(0x5C7A70)
    data = f.read(0x60)

print('ROM 0x005C7A70 .. 0x005C7AD0 (size 0x60):')
for i in range(0, len(data), 16):
    chunk = data[i:i+16]
    hexbytes = ' '.join(f'{b:02X}' for b in chunk)
    ascii_safe = ''.join(chr(b) if 0x20 <= b < 0x7F else '.' for b in chunk)
    print(f'  +0x{i:02X}: {hexbytes:<48}  |{ascii_safe}|')

# Check FRAGMENT magic at offset 8 (where it lives in real fragments)
magic = data[8:16]
print()
print(f'magic at +0x08: {magic!r}  (FRAGMENT magic = {b"FRAGMENT"!r})')

# Check if first instr looks like J
inst0 = struct.unpack('>I', data[:4])[0]
print(f'inst0 = 0x{inst0:08X}  J opcode = 0x{inst0 >> 26:02X} (J expected = 2)')
