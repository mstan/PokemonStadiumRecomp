"""Minimal F3DEX2 display-list decoder for offline analysis of a captured
DL .bin. Prints texture/tile/vertex/triangle/DL-control commands so we can
see how a draw is composed.

Usage: python tools/_gbi_decode.py <dl.bin> [max_cmds]
"""
import struct, sys

PATH = sys.argv[1]
MAXC = int(sys.argv[2]) if len(sys.argv) > 2 else 100000

with open(PATH, 'rb') as f:
    data = f.read()

FMT = {0: 'RGBA', 1: 'YUV', 2: 'CI', 3: 'IA', 4: 'I'}
SIZ = {0: '4b', 1: '8b', 2: '16b', 3: '32b'}

i = 0
n = 0
while i + 8 <= len(data) and n < MAXC:
    w0 = struct.unpack('>I', data[i:i+4])[0]
    w1 = struct.unpack('>I', data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    off = i
    i += 8
    n += 1
    if op == 0x01:  # G_VTX
        # w0: [op:8][?:4][numv:8 at bits 12-19? f3dex2: nn=(w0>>12)&0xff]
        numv = (w0 >> 12) & 0xFF
        vbidx = ((w0 >> 1) & 0x7F)
        print(f'+{off:#06x} G_VTX   n={numv} vbidx_end={vbidx} src=0x{w1:08X}')
    elif op == 0x05:  # G_TRI1
        a = ((w0 >> 16) & 0xFF) // 2
        b = ((w0 >> 8) & 0xFF) // 2
        c = (w0 & 0xFF) // 2
        print(f'+{off:#06x} G_TRI1  {a},{b},{c}')
    elif op == 0x06:  # G_TRI2
        a = ((w0 >> 16) & 0xFF) // 2; b = ((w0 >> 8) & 0xFF) // 2; c = (w0 & 0xFF) // 2
        d = ((w1 >> 16) & 0xFF) // 2; e = ((w1 >> 8) & 0xFF) // 2; ff = (w1 & 0xFF) // 2
        print(f'+{off:#06x} G_TRI2  {a},{b},{c} | {d},{e},{ff}')
    elif op == 0xFD:  # G_SETTIMG
        fmt = (w0 >> 21) & 0x7
        siz = (w0 >> 19) & 0x3
        wid = (w0 & 0xFFF) + 1
        print(f'+{off:#06x} G_SETTIMG fmt={FMT.get(fmt,fmt)} siz={SIZ.get(siz,siz)} w={wid} addr=0x{w1:08X}')
    elif op == 0xF5:  # G_SETTILE
        fmt = (w0 >> 21) & 0x7; siz = (w0 >> 19) & 0x3
        line = (w0 >> 9) & 0x1FF; tmem = w0 & 0x1FF
        tile = (w1 >> 24) & 0x7
        print(f'+{off:#06x} G_SETTILE tile={tile} fmt={FMT.get(fmt,fmt)} siz={SIZ.get(siz,siz)} line={line} tmem={tmem}')
    elif op == 0xF2:  # G_SETTILESIZE
        uls = (w0 >> 12) & 0xFFF; ult = w0 & 0xFFF
        tile = (w1 >> 24) & 0x7
        lrs = (w1 >> 12) & 0xFFF; lrt = w1 & 0xFFF
        print(f'+{off:#06x} G_SETTILESIZE tile={tile} uls={uls/4:.1f} ult={ult/4:.1f} lrs={lrs/4:.1f} lrt={lrt/4:.1f}')
    elif op == 0xF3:  # G_LOADBLOCK
        uls = (w0 >> 12) & 0xFFF; ult = w0 & 0xFFF
        tile = (w1 >> 24) & 0x7; lrs = (w1 >> 12) & 0xFFF; dxt = w1 & 0xFFF
        print(f'+{off:#06x} G_LOADBLOCK tile={tile} uls={uls} ult={ult} lrs={lrs} dxt={dxt}')
    elif op == 0xF4:  # G_LOADTILE
        uls = (w0 >> 12) & 0xFFF; ult = w0 & 0xFFF
        tile = (w1 >> 24) & 0x7; lrs = (w1 >> 12) & 0xFFF; lrt = w1 & 0xFFF
        print(f'+{off:#06x} G_LOADTILE tile={tile} uls={uls/4:.1f} ult={ult/4:.1f} lrs={lrs/4:.1f} lrt={lrt/4:.1f}')
    elif op == 0xDE:  # G_DL
        store = (w0 >> 16) & 0xFF
        print(f'+{off:#06x} G_DL    {"branch" if store else "call"} -> 0x{w1:08X}')
    elif op == 0xDF:  # G_ENDDL
        print(f'+{off:#06x} G_ENDDL')
    elif op == 0xE7:  # G_RDPPIPESYNC
        pass
    elif op == 0xED:  # G_SETSCISSOR
        ulx = (w0 >> 12) & 0xFFF; uly = w0 & 0xFFF
        lrx = (w1 >> 12) & 0xFFF; lry = w1 & 0xFFF
        print(f'+{off:#06x} G_SETSCISSOR ({ulx/4:.0f},{uly/4:.0f})-({lrx/4:.0f},{lry/4:.0f})')
    elif op == 0xDB:  # G_MOVEWORD / viewport handled elsewhere
        print(f'+{off:#06x} G_MOVEMEM idx={w0 & 0xFF} addr=0x{w1:08X}')
    elif op == 0xFA:  # G_SETPRIMCOLOR
        print(f'+{off:#06x} G_SETPRIMCOLOR rgba={(w1>>24)&0xFF},{(w1>>16)&0xFF},{(w1>>8)&0xFF},{w1&0xFF}')
    # else: skip silently (combine/othermode/etc.)

print(f'\n[decoded {n} cmds over {i} bytes of {len(data)}]')
