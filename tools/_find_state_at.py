"""Walk DL up to a given TEXRECT offset and report the latest
SETTIMG, SETTILE (per tile), SETCOMBINE, and SETPRIMCOLOR/ENVCOLOR
in effect at that point."""
import struct, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_dl_gpk_now.bin"
TARGET = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x7150

with open(PATH, "rb") as f:
    data = f.read()

FMT = {0:"RGBA",1:"YUV",2:"CI",3:"IA",4:"I"}
SIZ = {0:"4b",1:"8b",2:"16b",3:"32b"}

timg = None
tiles = [None]*8
sizes = [None]*8
loads = [None]*8
combine = (0,0)
prim = env = 0

i = 0
while i < TARGET and i+8 <= len(data):
    w0 = struct.unpack(">I", data[i:i+4])[0]
    w1 = struct.unpack(">I", data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    if op == 0xFD:
        timg = ((w0>>21)&7, (w0>>19)&3, (w0&0xFFF)+1, w1, i)
    elif op == 0xF5:
        tiles[(w1>>24)&7] = (w0, w1, i)
    elif op == 0xF2:
        sizes[(w1>>24)&7] = (w0, w1, i)
    elif op == 0xF3:
        loads[(w1>>24)&7] = (w0, w1, timg, i)
    elif op == 0xFC:
        combine = (w0, w1)
    elif op == 0xFA:
        prim = w1
    elif op == 0xFB:
        env = w1
    i += 8

print(f"State at offset {TARGET:#06x}:")
print(f"  combine: 0x{combine[0]:08x} 0x{combine[1]:08x}")
print(f"  prim RGBA: ({(prim>>24)&0xFF},{(prim>>16)&0xFF},{(prim>>8)&0xFF},{prim&0xFF})")
print(f"  env  RGBA: ({(env>>24)&0xFF},{(env>>16)&0xFF},{(env>>8)&0xFF},{env&0xFF})")

if timg:
    fmt, siz, w, addr, off = timg
    print(f"  SETTIMG (last set at +{off:#06x}): fmt={FMT.get(fmt,'?')}/{SIZ.get(siz,'?')} width={w} addr=0x{addr:08x}")

# Look at the texrect's tile_idx
w0t = struct.unpack(">I", data[TARGET:TARGET+4])[0]
w1t = struct.unpack(">I", data[TARGET+4:TARGET+8])[0]
tile_idx = (w1t>>24)&7
print(f"\nTexrect at +{TARGET:#06x} uses tile {tile_idx}")
if tiles[tile_idx]:
    tw0, tw1, toff = tiles[tile_idx]
    fmt = (tw0>>21)&7; siz = (tw0>>19)&3; line = (tw0>>9)&0x1FF; tmem = tw0&0x1FF
    print(f"  tile{tile_idx} (set at +{toff:#06x}): fmt={FMT.get(fmt,'?')}/{SIZ.get(siz,'?')} line={line} tmem={tmem}")
if sizes[tile_idx]:
    sw0, sw1, soff = sizes[tile_idx]
    uls=(sw0>>12)&0xFFF; ult=sw0&0xFFF
    lrs=(sw1>>12)&0xFFF; lrt=sw1&0xFFF
    print(f"  tilesize (set at +{soff:#06x}): uls={uls/4.0} ult={ult/4.0} lrs={lrs/4.0} lrt={lrt/4.0}  ({(lrs-uls)/4+1}x{(lrt-ult)/4+1} tex)")

# Find the most recent LOAD that filled this tile's TMEM
# (any tile, since they might use different tile for LOAD vs SAMPLE)
print(f"\nMost recent loads (any tile):")
for ti in range(8):
    if loads[ti]:
        lw0, lw1, lt, loff = loads[ti]
        if lt is None: continue
        ulrs = (lw1 >> 12) & 0xFFF
        dxt = lw1 & 0xFFF
        print(f"  tile{ti} loadblock (at +{loff:#06x}): lrs={ulrs} dxt={dxt}  via TIMG fmt={FMT.get(lt[0],'?')}/{SIZ.get(lt[1],'?')} addr=0x{lt[3]:08x}")
