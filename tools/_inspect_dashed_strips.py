"""Walk a captured DL; for every NON_UNIT_UV TEXRECT (i.e. likely a
stretched 1D strip — dashed-line suspect), print the most recent
combiner/blender/prim/env state and tile/load info.

Goal: find what's producing the dashed yellow lines on Game Pak
Check controller-panel borders. With dsdx=0 dtdy=1 (or vice
versa), pure texture sampling can't produce horizontal dashes —
so the source must be combiner state, prim alpha pattern, or
overlaid texrects.

F3DEX2 ops we care about beyond the existing set:
  G_SETCOMBINE     (0xFC) — color/alpha cycle 1/2 sources
  G_RDPSETOTHERMODE(0xEF) — bl/cvg/render mode
  G_SETPRIMCOLOR   (0xFA) — prim RGBA + lod
  G_SETENVCOLOR    (0xFB) — env RGBA
  G_SETFOGCOLOR    (0xF8)
  G_SETBLENDCOLOR  (0xF9)
  G_RDPHALF_1/2    (0xE1/0xF1)
"""
import struct, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_dl_gamepakcheck.bin"

with open(PATH, "rb") as f:
    data = f.read()

FMT = {0:"RGBA",1:"YUV",2:"CI",3:"IA",4:"I"}
SIZ = {0:"4b",1:"8b",2:"16b",3:"32b"}

# Cycle1/2 combiner source LUTs (F3DEX2 SETCOMBINE encoding).
A_C  = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","1","COMB1","T0a","T1a","Pa","Sa","Ea","LOD","P_LOD","K5"]
B_C  = A_C
C_C  = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","SCL","COMB1","T0a","T1a","Pa","Sa","Ea","LOD","P_LOD","K5","?","?","?","?","?","?","?","?","?","?","?","?","?","?","?","0"]
D_C  = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","1","0"]
A_A  = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","1","0"]
D_A  = A_A

def decode_combine(w0, w1):
    # w0: a0(20:16) c0(15:11) Aa0(10:8)  Ac0?  (this is a packed 8-field cycle1)
    # Easier to use the canonical layout:
    # bits63..0:
    #   a0  20:23 in w0
    #   c0  15:19 in w0
    #   Aa0 12:14 in w0
    #   Ac0 9:11  in w0
    #   a1  5:7   in w0
    #   Aa1 0:2   in w0  (these wrap into w1 too)
    # ...full SETCOMBINE bit layout is tricky; just dump raw.
    return f"raw=0x{w0:08x} 0x{w1:08x}"

# State trackers
combine = (0,0)
otherL = otherH = 0
prim = env = 0
fog = blend = 0
lastTimg = (0,0,0,0)  # fmt,siz,width,addr
lastTile = [None]*8
lastSize = [None]*8
lastLoad = [None]*8

def ev(off, kind, **kv):
    return {"off":off, "kind":kind, **kv}

events = []
i = 0
while i + 8 <= len(data):
    w0 = struct.unpack(">I", data[i:i+4])[0]
    w1 = struct.unpack(">I", data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    if op == 0xFC:
        combine = (w0,w1)
        events.append(ev(i, "COMBINE", w0=w0, w1=w1))
    elif op == 0xEF:
        otherL = w0 & 0x00FFFFFF
        otherH = w1
        events.append(ev(i, "OTHERMODE", l=otherL, h=otherH))
    elif op == 0xFA:
        prim = w1
        prim_min = (w0 >> 8) & 0xFF
        prim_lod = w0 & 0xFF
        events.append(ev(i, "PRIM", rgba=w1, min=prim_min, lod=prim_lod))
    elif op == 0xFB:
        env = w1
        events.append(ev(i, "ENV", rgba=w1))
    elif op == 0xF8:
        fog = w1
        events.append(ev(i, "FOG", rgba=w1))
    elif op == 0xF9:
        blend = w1
        events.append(ev(i, "BLEND", rgba=w1))
    elif op == 0xFD:
        lastTimg = ((w0>>21)&7, (w0>>19)&3, (w0&0xFFF)+1, w1)
    elif op == 0xF5:
        tile = (w1>>24)&7
        lastTile[tile] = (w0,w1)
    elif op == 0xF2:
        tile = (w1>>24)&7
        lastSize[tile] = (w0,w1)
    elif op == 0xF3:
        tile = (w1>>24)&7
        lastLoad[tile] = (w0,w1, lastTimg)
    elif op == 0xE4:
        lrx = (w0 >> 12) & 0xFFF
        lry = w0 & 0xFFF
        tile_idx = (w1 >> 24) & 0x7
        ulx = (w1 >> 12) & 0xFFF
        uly = w1 & 0xFFF
        m1 = struct.unpack(">I", data[i+20:i+24])[0]  if i+24<=len(data) else 0
        dsdx = (m1 >> 16) & 0xFFFF
        dtdy = m1 & 0xFFFF
        def s5_10(x):
            x &= 0xFFFF
            if x & 0x8000: x -= 0x10000
            return x / 1024.0
        dsdx_f = s5_10(dsdx)
        dtdy_f = s5_10(dtdy)
        events.append(ev(i, "TEXRECT",
            ulx=ulx/4.0, uly=uly/4.0, lrx=lrx/4.0, lry=lry/4.0,
            tile=tile_idx, dsdx=dsdx_f, dtdy=dtdy_f,
            combine=combine, prim=prim, env=env,
            otherL=otherL, otherH=otherH,
            tilereg=lastTile[tile_idx],
            sizereg=lastSize[tile_idx],
            loadreg=lastLoad[tile_idx]))
    i += 8

texrects = [e for e in events if e["kind"] == "TEXRECT"]
print(f"DL: {PATH}  TEXRECTs: {len(texrects)}\n")

suspect = [e for e in texrects if (e["dsdx"] != 1.0 or e["dtdy"] != 1.0)]
print(f"NON_UNIT_UV (likely stretched 1D strips): {len(suspect)}\n")

def fmt_rgba(x):
    return f"({(x>>24)&0xFF:3d},{(x>>16)&0xFF:3d},{(x>>8)&0xFF:3d},{x&0xFF:3d})"

# Group by combine state — likely all corrupt strips share one combine
groups = {}
for e in suspect:
    key = e["combine"]
    groups.setdefault(key, []).append(e)

for key, lst in sorted(groups.items(), key=lambda kv: -len(kv[1])):
    w0, w1 = key
    print(f"=== combine raw=0x{w0:08x} 0x{w1:08x}  ({len(lst)} texrects) ===")
    e0 = lst[0]
    print(f"  prim={fmt_rgba(e0['prim'])}  env={fmt_rgba(e0['env'])}  otherL=0x{e0['otherL']:06x}  otherH=0x{e0['otherH']:08x}")
    if e0["tilereg"]:
        tw0, tw1 = e0["tilereg"]
        fmt = (tw0 >> 21) & 0x7
        siz = (tw0 >> 19) & 0x3
        line = (tw0 >> 9) & 0x1FF
        tmem = tw0 & 0x1FF
        cms = (tw1 >> 8) & 0x3
        masks = (tw1 >> 4) & 0xF
        cmt = (tw1 >> 18) & 0x3
        maskt = (tw1 >> 14) & 0xF
        print(f"  tile fmt={FMT.get(fmt,'?')}/{SIZ.get(siz,'?')}  line={line}  tmem={tmem}  cms/maskS={cms}/{masks}  cmt/maskT={cmt}/{maskt}")
    if e0["loadreg"]:
        lw0, lw1, ltimg = e0["loadreg"]
        ulrs = (lw1 >> 12) & 0xFFF
        dxt = lw1 & 0xFFF
        print(f"  load lrs={ulrs} dxt={dxt}  timg=0x{ltimg[3]:08x} {FMT.get(ltimg[0],'?')}/{SIZ.get(ltimg[1],'?')} w={ltimg[2]}")
    for e in lst[:6]:
        sx, sy = e["ulx"], e["uly"]
        sw, sh = e["lrx"]-e["ulx"], e["lry"]-e["uly"]
        print(f"  +{e['off']:#06x}  scr=({sx:6.1f},{sy:6.1f}) {sw:5.1f}x{sh:4.1f}  dsdx/dtdy={e['dsdx']:.2f}/{e['dtdy']:.2f}")
    if len(lst) > 6:
        print(f"  ... +{len(lst)-6} more")
    print()
