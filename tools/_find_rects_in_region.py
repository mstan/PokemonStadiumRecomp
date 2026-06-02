"""Find texrects whose screen rect intersects the given screen region
(in native 320x240-ish coords). Group them by approximate position
to expose the tile-grid structure of large panels.
"""
import struct, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_dl_gpk_now.bin"
X_LO = float(sys.argv[2]) if len(sys.argv) > 2 else 30
Y_LO = float(sys.argv[3]) if len(sys.argv) > 3 else 130
X_HI = float(sys.argv[4]) if len(sys.argv) > 4 else 600
Y_HI = float(sys.argv[5]) if len(sys.argv) > 5 else 330

with open(PATH, "rb") as f:
    data = f.read()

texrects = []
i = 0
while i+8 <= len(data):
    w0 = struct.unpack(">I", data[i:i+4])[0]
    w1 = struct.unpack(">I", data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    if op == 0xE4:
        lrx = (w0 >> 12) & 0xFFF
        lry = w0 & 0xFFF
        ulx = (w1 >> 12) & 0xFFF
        uly = w1 & 0xFFF
        m1 = struct.unpack(">I", data[i+20:i+24])[0]  if i+24<=len(data) else 0
        dsdx = (m1 >> 16) & 0xFFFF
        dtdy = m1 & 0xFFFF
        def s5_10(x):
            x &= 0xFFFF
            if x & 0x8000: x -= 0x10000
            return x / 1024.0
        texrects.append({
            "off": i,
            "ulx": ulx/4.0, "uly": uly/4.0, "lrx": lrx/4.0, "lry": lry/4.0,
            "dsdx": s5_10(dsdx), "dtdy": s5_10(dtdy),
        })
    i += 8

# Filter to region
def overlaps(a_lo, a_hi, b_lo, b_hi):
    return max(a_lo, b_lo) < min(a_hi, b_hi)

hits = [t for t in texrects
        if overlaps(t["ulx"], t["lrx"], X_LO, X_HI)
        and overlaps(t["uly"], t["lry"], Y_LO, Y_HI)]
print(f"DL: {PATH}")
print(f"region X={X_LO}..{X_HI}  Y={Y_LO}..{Y_HI}  hits: {len(hits)}\n")

# Sort by Y then X
hits.sort(key=lambda t: (t["uly"], t["ulx"]))
for t in hits:
    print(f"  +{t['off']:#06x}  ({t['ulx']:6.1f},{t['uly']:6.1f})-({t['lrx']:6.1f},{t['lry']:6.1f})  "
          f"{t['lrx']-t['ulx']:5.1f}x{t['lry']-t['uly']:4.1f}  dsdx/dtdy={t['dsdx']:5.2f}/{t['dtdy']:5.2f}")
