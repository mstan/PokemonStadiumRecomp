"""Find pairs of TEXRECTs in a captured DL that should be adjacent
(share an edge) and report whether their coords line up exactly
or have a gap.
"""
import struct, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_dl_gamepakcheck.bin"
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
        texrects.append({
            "off": i,
            "ulx": ulx/4.0, "uly": uly/4.0,
            "lrx": lrx/4.0, "lry": lry/4.0,
        })
    i += 8

# For each texrect, find any other texrect whose ulx == this.lrx (right neighbor)
# or whose uly == this.lry (bottom neighbor) and overlaps in the orthogonal axis.
# Also report any pair with small gap (1-4 px) but otherwise abuts.
def overlaps(a_lo, a_hi, b_lo, b_hi):
    return max(a_lo, b_lo) < min(a_hi, b_hi)

print(f"DL: {PATH}  TEXRECTs: {len(texrects)}\n")
print("Right-neighbor pairs (B starts immediately right of A):\n")
for i, a in enumerate(texrects):
    for j, b in enumerate(texrects):
        if i == j: continue
        # Check Y overlap
        if not overlaps(a["uly"], a["lry"], b["uly"], b["lry"]):
            continue
        # Check X relationship
        gap = b["ulx"] - a["lrx"]
        if -0.01 <= gap <= 4.01 and a["ulx"] < b["ulx"]:
            tag = "ABUT" if abs(gap) < 0.01 else f"GAP={gap:.2f}px"
            print(f"  +{a['off']:#06x} ({a['ulx']:.1f},{a['uly']:.1f})-({a['lrx']:.1f},{a['lry']:.1f})  ->  "
                  f"+{b['off']:#06x} ({b['ulx']:.1f},{b['uly']:.1f})-({b['lrx']:.1f},{b['lry']:.1f})   [{tag}]")

print("\nBottom-neighbor pairs (B starts immediately below A):\n")
for i, a in enumerate(texrects):
    for j, b in enumerate(texrects):
        if i == j: continue
        if not overlaps(a["ulx"], a["lrx"], b["ulx"], b["lrx"]):
            continue
        gap = b["uly"] - a["lry"]
        if -0.01 <= gap <= 4.01 and a["uly"] < b["uly"]:
            tag = "ABUT" if abs(gap) < 0.01 else f"GAP={gap:.2f}px"
            print(f"  +{a['off']:#06x} ({a['ulx']:.1f},{a['uly']:.1f})-({a['lrx']:.1f},{a['lry']:.1f})  ->  "
                  f"+{b['off']:#06x} ({b['ulx']:.1f},{b['uly']:.1f})-({b['lrx']:.1f},{b['lry']:.1f})   [{tag}]")
