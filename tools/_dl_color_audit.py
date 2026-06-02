"""Walk a captured DL and list unique SETPRIMCOLOR + SETENVCOLOR
values along with the subsequent TEXRECT's screen region. Goal: see
what yellow-colored draws are happening on the Game Pak Check screen
that might explain the residual Player 3 lines.
"""
import struct, sys, collections

PATH = sys.argv[1] if len(sys.argv) > 1 else 'F:/Projects/n64recomp/PokemonStadiumRecomp/build/current_gamepak_dl.bin'

with open(PATH, 'rb') as f:
    data = f.read()

prim = 0
env = 0
combine = (0, 0)
events = []

i = 0
while i + 8 <= len(data):
    w0 = struct.unpack('>I', data[i:i+4])[0]
    w1 = struct.unpack('>I', data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    if op == 0xFA:
        prim = w1
    elif op == 0xFB:
        env = w1
    elif op == 0xFC:
        combine = (w0, w1)
    elif op == 0xE4:  # TEXRECT
        lrx = (w0 >> 12) & 0xFFF
        lry = w0 & 0xFFF
        ulx = (w1 >> 12) & 0xFFF
        uly = w1 & 0xFFF
        events.append((i, ulx/4.0, uly/4.0, lrx/4.0, lry/4.0, prim, env, combine))
    i += 8

print(f'DL {PATH}: {len(events)} TEXRECTs\n')

# Group by prim color
groups = collections.defaultdict(list)
for ev in events:
    groups[ev[5]].append(ev)

for primv, lst in sorted(groups.items(), key=lambda kv: -len(kv[1])):
    pr, pg, pb, pa = (primv >> 24) & 0xFF, (primv >> 16) & 0xFF, (primv >> 8) & 0xFF, primv & 0xFF
    is_yellow = pr >= 200 and pg >= 200 and pb < 128
    marker = '  *** YELLOW ***' if is_yellow else ''
    print(f'PRIM=({pr:3d},{pg:3d},{pb:3d},{pa:3d}) hex=0x{primv:08X}  {len(lst)} rects{marker}')
    for off, ulx, uly, lrx, lry, _, env_v, comb in lst[:8]:
        env_r, env_g, env_b, env_a = (env_v >> 24) & 0xFF, (env_v >> 16) & 0xFF, (env_v >> 8) & 0xFF, env_v & 0xFF
        print(f'  +{off:#06x}  scr=({ulx:6.1f},{uly:6.1f})-({lrx:6.1f},{lry:6.1f})  '
              f'w={lrx-ulx:5.1f} h={lry-uly:4.1f}  env=({env_r},{env_g},{env_b},{env_a})')
    if len(lst) > 8:
        print(f'  ... +{len(lst)-8} more')
