"""List every SETCOMBINE invocation in a DL with offset and decode."""
import struct, sys

PATH = sys.argv[1]
data = open(PATH,'rb').read()

A = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","1","NOISE","0","0","0","0","0","0","0","0"]
B = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","CCEN","K4","0","0","0","0","0","0","0","0"]
C = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","SCL","COMB1A","T0a","T1a","Pa","Sa","Ea","LOD","P_LOD","K5"] + ["0"]*16
D = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","1","0"]
Aa= D
Ca= ["LOD","TEX0","TEX1","PRIM","SHADE","ENV","P_LOD","0"]

def decode(w0, w1):
    a0=(w0>>20)&0xF; c0=(w0>>15)&0x1F; Aa0=(w0>>12)&0x7; Ac0=(w0>>9)&0x7
    a1=(w0>>5)&0xF;  c1=w0&0x1F
    b0=(w1>>28)&0xF; b1=(w1>>24)&0xF
    Aa1=(w1>>21)&0x7; Ac1=(w1>>18)&0x7
    d0=(w1>>15)&0x7; Ab0=(w1>>12)&0x7; Ad0=(w1>>9)&0x7
    d1=(w1>>6)&0x7;  Ab1=(w1>>3)&0x7;  Ad1=w1&0x7
    return (
        f"({A[a0]}-{B[b0]})*{C[c0]}+{D[d0]} | A:({Aa[Aa0]}-{Aa[Ab0]})*{Ca[Ac0]}+{Aa[Ad0]} || "
        f"({A[a1]}-{B[b1]})*{C[c1]}+{D[d1]} | A:({Aa[Aa1]}-{Aa[Ab1]})*{Ca[Ac1]}+{Aa[Ad1]}"
    )

i = 0
combines = []
while i+8 <= len(data):
    w0 = struct.unpack(">I", data[i:i+4])[0]
    w1 = struct.unpack(">I", data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    if op == 0xFC:
        combines.append((i, w0, w1))
    i += 8

# Group by raw value
from collections import defaultdict
groups = defaultdict(list)
for off, w0, w1 in combines:
    groups[(w0,w1)].append(off)

print(f"DL: {PATH}  SETCOMBINE invocations: {len(combines)}  unique: {len(groups)}\n")
for (w0,w1), offs in sorted(groups.items(), key=lambda kv: -len(kv[1])):
    print(f"raw=0x{w0:08x} 0x{w1:08x}  ({len(offs)} sites)")
    print(f"  {decode(w0,w1)}")
    print(f"  first sites: {[hex(o) for o in offs[:5]]}{'...' if len(offs)>5 else ''}")
    print()
