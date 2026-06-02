A = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","1","NOISE","0","0","0","0","0","0","0","0"]
B = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","CCEN","K4","0","0","0","0","0","0","0","0"]
C = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","SCALE","COMB1A",
     "T0a","T1a","Pa","Sa","Ea","LOD_F","P_LOD_F","K5",
     "0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0"]
D = ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","1","0"]
Aa= ["COMB","TEX0","TEX1","PRIM","SHADE","ENV","1","0"]
Ca= ["LOD","TEX0","TEX1","PRIM","SHADE","ENV","P_LOD","0"]

import sys
w0 = int(sys.argv[1], 16)
w1 = int(sys.argv[2], 16)

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
print(f"cycle1 RGB: ({A[a0]} - {B[b0]}) * {C[c0]} + {D[d0]}")
print(f"cycle1 A  : ({Aa[Aa0]} - {Aa[Ab0]}) * {Ca[Ac0]} + {Aa[Ad0]}")
print(f"cycle2 RGB: ({A[a1]} - {B[b1]}) * {C[c1]} + {D[d1]}")
print(f"cycle2 A  : ({Aa[Aa1]} - {Aa[Ab1]}) * {Ca[Ac1]} + {Aa[Ad1]}")
