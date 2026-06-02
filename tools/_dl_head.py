"""Dump raw words of a captured DL for low-level inspection."""
import struct, sys
PATH = sys.argv[1] if len(sys.argv) > 1 else 'F:/Projects/n64recomp/PokemonStadiumRecomp/build/dl_mainmenu_clip.bin'
N = int(sys.argv[2]) if len(sys.argv) > 2 else 32  # n commands

with open(PATH, 'rb') as f:
    data = f.read()
for i in range(0, min(N * 8, len(data)), 8):
    w0, w1 = struct.unpack('>II', data[i:i+8])
    op = (w0 >> 24) & 0xFF
    print(f'+{i:#06x}  w0={w0:08X} w1={w1:08X}  op=0x{op:02X}')
