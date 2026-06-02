"""Walk the DL and for one specific dashed-strip texrect at the
given offset, dump every preceding RDP cmd back to the previous
TEXRECT (or start). Also decode SETCOMBINE properly.
"""
import struct, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_dl_gamepakcheck.bin"
TARGET = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x0450

with open(PATH, "rb") as f:
    data = f.read()

FMT = {0:"RGBA",1:"YUV",2:"CI",3:"IA",4:"I"}
SIZ = {0:"4b",1:"8b",2:"16b",3:"32b"}

A = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","1","NOISE","0","0","0","0","0","0","0","0"]
B = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","CCEN","K4","0","0","0","0","0","0","0","0"]
C = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","SCALE","COMB1A",
     "T0a","T1a","Pa","Sa","Ea","LOD_F","P_LOD_F","K5",
     "0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0"]
D = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","1","0"]
Aa= ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","1","0"]
Ba= Aa
Ca= ["LOD","TEX0","TEX1","PRIM","SHADE","ENV","P_LOD","0"]
Da= Aa

def decode_combine(w0, w1):
    a0  = (w0 >> 20) & 0xF
    c0  = (w0 >> 15) & 0x1F
    Aa0 = (w0 >> 12) & 0x7
    Ac0 = (w0 >> 9)  & 0x7
    a1  = (w0 >> 5)  & 0xF
    c1  = (w0 >> 0)  & 0x1F
    b0  = (w1 >> 28) & 0xF
    b1  = (w1 >> 24) & 0xF
    Aa1 = (w1 >> 21) & 0x7
    Ac1 = (w1 >> 18) & 0x7
    d0  = (w1 >> 15) & 0x7
    Ab0 = (w1 >> 12) & 0x7
    Ad0 = (w1 >> 9)  & 0x7
    d1  = (w1 >> 6)  & 0x7
    Ab1 = (w1 >> 3)  & 0x7
    Ad1 = (w1 >> 0)  & 0x7
    return (
        f"  cycle1 RGB:  ({A[a0]} - {B[b0]}) * {C[c0]} + {D[d0]}\n"
        f"  cycle1 A:    ({Aa[Aa0]} - {Ba[Ab0]}) * {Ca[Ac0]} + {Da[Ad0]}\n"
        f"  cycle2 RGB:  ({A[a1]} - {B[b1]}) * {C[c1]} + {D[d1]}\n"
        f"  cycle2 A:    ({Aa[Aa1]} - {Ba[Ab1]}) * {Ca[Ac1]} + {Da[Ad1]}\n"
    )

# Find offset of target texrect; collect indices of prior texrects.
texrect_offs = []
i = 0
while i+8 <= len(data):
    op = (data[i] >> 0)  # op is byte 0 of w0
    if op == 0xE4:
        texrect_offs.append(i)
        if i == TARGET:
            break
    i += 8

if TARGET not in texrect_offs:
    print(f"TEXRECT at offset 0x{TARGET:04x} not found. Texrect offsets: {[hex(x) for x in texrect_offs[:20]]}...")
    sys.exit(1)

idx = texrect_offs.index(TARGET)
prev_off = texrect_offs[idx-1] if idx > 0 else 0
print(f"Dumping cmds from {prev_off:#06x} (prev TEXRECT) up to and including target {TARGET:#06x}\n")

i = prev_off
end = TARGET + 24  # include the texrect's RDPHALF_1+RDPHALF_2 (24 bytes total: 8 cmd + 16 trail)
while i < end and i+8 <= len(data):
    w0 = struct.unpack(">I", data[i:i+4])[0]
    w1 = struct.unpack(">I", data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    name = {
        0xE4:"TEXRECT", 0xE5:"TEXRECT_FLIP", 0xFC:"SETCOMBINE",
        0xFB:"SETENVCOLOR", 0xFA:"SETPRIMCOLOR", 0xF9:"SETBLENDCOLOR",
        0xF8:"SETFOGCOLOR", 0xF7:"SETFILLCOLOR", 0xF6:"FILLRECT",
        0xF5:"SETTILE", 0xF4:"LOADTILE", 0xF3:"LOADBLOCK",
        0xF2:"SETTILESIZE", 0xF1:"RDPHALF_2", 0xE1:"RDPHALF_1",
        0xFD:"SETTIMG", 0xEF:"RDPSETOTHERMODE",
        0xE3:"SETOTHERMODE_H", 0xE2:"SETOTHERMODE_L",
        0xD7:"G_TEXTURE", 0xE6:"RDPLOADSYNC", 0xE7:"RDPPIPESYNC",
        0xE8:"RDPTILESYNC", 0xE9:"RDPFULLSYNC", 0xDF:"ENDDL",
        0xDE:"DL", 0xD8:"POPMTX", 0xDA:"MTX", 0xDB:"MOVEWORD", 0xDC:"MOVEMEM",
        0xDD:"LOAD_UCODE", 0xDF:"ENDDL", 0xE0:"SPNOOP",
        0x06:"branch_z", 0x07:"tri2", 0xBC:"MOVEWORD",  # F3DEX2 alt
    }.get(op, f"op={op:#04x}")
    print(f"+{i:#06x}  {name:18s}  w0=0x{w0:08x}  w1=0x{w1:08x}")
    if op == 0xFC:
        print(decode_combine(w0, w1), end="")
    elif op == 0xFA:
        print(f"    prim RGBA = ({(w1>>24)&0xFF},{(w1>>16)&0xFF},{(w1>>8)&0xFF},{w1&0xFF})  prim_min={(w0>>8)&0xFF}  lod={w0&0xFF}")
    elif op == 0xFB:
        print(f"    env  RGBA = ({(w1>>24)&0xFF},{(w1>>16)&0xFF},{(w1>>8)&0xFF},{w1&0xFF})")
    elif op == 0xFD:
        print(f"    fmt={FMT.get((w0>>21)&7,'?')}/{SIZ.get((w0>>19)&3,'?')}  width={(w0&0xFFF)+1}  addr=0x{w1:08x}")
    elif op == 0xF5:
        fmt=(w0>>21)&7; siz=(w0>>19)&3; line=(w0>>9)&0x1FF; tmem=w0&0x1FF
        tile=(w1>>24)&7; pal=(w1>>20)&0xF
        cmt=(w1>>18)&3; maskt=(w1>>14)&0xF; shiftt=(w1>>10)&0xF
        cms=(w1>>8)&3; masks=(w1>>4)&0xF; shifts=w1&0xF
        print(f"    tile={tile}  fmt={FMT.get(fmt,'?')}/{SIZ.get(siz,'?')}  line={line}  tmem={tmem}  pal={pal}")
        print(f"        cms/masks/shifts={cms}/{masks}/{shifts}  cmt/maskt/shiftt={cmt}/{maskt}/{shiftt}")
    elif op == 0xF2:
        uls=(w0>>12)&0xFFF; ult=w0&0xFFF; tile=(w1>>24)&7
        lrs=(w1>>12)&0xFFF; lrt=w1&0xFFF
        print(f"    tile={tile}  uls={uls/4.0} ult={ult/4.0} lrs={lrs/4.0} lrt={lrt/4.0}  ({(lrs-uls)/4+1}x{(lrt-ult)/4+1} texels)")
    elif op == 0xF3:
        uls=(w0>>12)&0xFFF; ult=w0&0xFFF; tile=(w1>>24)&7
        lrs=(w1>>12)&0xFFF; dxt=w1&0xFFF
        print(f"    tile={tile}  uls={uls} ult={ult} lrs={lrs} dxt={dxt}  ({lrs+1} texels at TIMG-fmt size)")
    elif op == 0xE3:
        print(f"    mode_h shift={(w0>>8)&0xFF} len={(w0&0xFF)+1}  data=0x{w1:08x}")
    elif op == 0xE2:
        print(f"    mode_l shift={(w0>>8)&0xFF} len={(w0&0xFF)+1}  data=0x{w1:08x}")
    elif op == 0xE4:
        lrx=(w0>>12)&0xFFF; lry=w0&0xFFF
        tile=(w1>>24)&7; ulx=(w1>>12)&0xFFF; uly=w1&0xFFF
        print(f"    tile={tile}  ulx={ulx/4.0} uly={uly/4.0} lrx={lrx/4.0} lry={lry/4.0}  ({(lrx-ulx)/4}x{(lry-uly)/4} px)")
    elif op == 0xE1 or op == 0xF1:
        s=(w1>>16)&0xFFFF; t=w1&0xFFFF
        sf = (s if s<0x8000 else s-0x10000)/32.0
        tf = (t if t<0x8000 else t-0x10000)/32.0
        print(f"    {'s/t' if op==0xE1 else 'dsdx/dtdy'}  raw=({s:#06x},{t:#06x})  decoded=({sf:.3f},{tf:.3f})")
    i += 8
