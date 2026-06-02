"""Decode the FRAGMENT header in build/hash_miss_id_*.bin to identify
which recompiled variant the live bytes actually came from.

Header layout (BE 32-bit words):
  +0x00 J trampoline (top 6 bits = 0x02 opcode)
  +0x04 nop / delay slot
  +0x08 magic_a = 'FRAG' (0x46524147)
  +0x0C magic_b = 'MENT' (0x4D454E54)
  +0x10 entry_off
  +0x14 sizeInRam (post-decompression / loaded fragment)

J target = ((j_instr & 0x03FFFFFF) << 2) | 0x80000000.
link_vram = j_target - entry_off  (where the recompiler emitted this fragment).
"""
import struct
import sys
import glob

paths = sorted(glob.glob('F:/Projects/n64recomp/PokemonStadiumRecomp/build/hash_miss_id_*.bin'))
if not paths:
    print('no hash_miss_*.bin files', file=sys.stderr); sys.exit(1)

for path in paths:
    data = open(path, 'rb').read()
    print(f'== {path} ==')
    print(f'  total bytes: {len(data)}')
    if len(data) < 0x20:
        print('  too small for header'); continue
    hdr = struct.unpack('>8I', data[:0x20])
    print(f'  +0x00 J_instr   = 0x{hdr[0]:08X}')
    print(f'  +0x04 delay     = 0x{hdr[1]:08X}')
    a, b = hdr[2], hdr[3]
    def to_ascii(v):
        return ''.join(chr((v >> (24 - 8*i)) & 0xFF) if 32 <= ((v >> (24 - 8*i)) & 0xFF) <= 126 else '.' for i in range(4))
    print(f'  +0x08 magic_a   = 0x{a:08X}  "{to_ascii(a)}"')
    print(f'  +0x0C magic_b   = 0x{b:08X}  "{to_ascii(b)}"')
    print(f'  +0x10 entry_off = 0x{hdr[4]:08X}')
    print(f'  +0x14 sizeInRam = 0x{hdr[5]:08X}')
    j = hdr[0]
    op = (j >> 26) & 0x3F
    target = ((j & 0x03FFFFFF) << 2) | 0x80000000
    print(f'  J: op={op:#x} target=0x{target:08X}')
    if op == 0x02 and a == 0x46524147 and b == 0x4D454E54:
        link = target - hdr[4]
        print(f'  ✓ valid FRAGMENT header: link_vram = 0x{link:08X}')
    else:
        print(f'  ✗ header magic does NOT match — these bytes do not start with a FRAGMENT trampoline')
        print(f'    First 32 bytes: ' + ' '.join(f'{b:02X}' for b in data[:32]))
