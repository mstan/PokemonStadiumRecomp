"""Simulate RT64's drawRect -> viewport conversion and the GPU's
top-left rasterization rule. Check whether abutting texrect pairs
in a captured DL produce gapped or overlapping pixel coverage at
various render resolution scales.

RT64's logic (lib/rt64/src/render/rt64_framebuffer_renderer.cpp:107-110):
  left   = round((mid + (rect.left(ceil)  - mid) * aspectScale) * resScale.x)
  right  = round((mid + (rect.right(ceil) - mid) * aspectScale) * resScale.x)
  top    = round(rect.top(ceil)    * resScale.y)
  bottom = round(rect.bottom(ceil) * resScale.y)
With aspectScale=1, mid=fbWidth/2, this is just round(coord * res).
The vertex shader then applies a half-pixel offset; the rect renders
pixels in [left, right) x [top, bottom) under top-left rule.
"""
import struct, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_dl_gpk_now.bin"

with open(PATH, "rb") as f:
    data = f.read()

texrects = []
i = 0
while i+8 <= len(data):
    w0 = struct.unpack(">I", data[i:i+4])[0]
    w1 = struct.unpack(">I", data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    if op == 0xE4:
        # 10.2 fixed-point coords
        lrx_fx = (w0 >> 12) & 0xFFF
        lry_fx = w0 & 0xFFF
        ulx_fx = (w1 >> 12) & 0xFFF
        uly_fx = w1 & 0xFFF
        # FixedRect coords are in 10.2 fixed: store as int32 = (raw fixed-point pixel value)
        # FixedRect.left(ceil=true) = (ulx_fx + 3) >> 2
        ulx_pix = (ulx_fx + 3) >> 2  # pixel-int after ceil
        uly_pix = (uly_fx + 3) >> 2
        lrx_pix = (lrx_fx + 3) >> 2
        lry_pix = (lry_fx + 3) >> 2
        texrects.append({
            "off": i,
            "ulx_fx": ulx_fx, "uly_fx": uly_fx, "lrx_fx": lrx_fx, "lry_fx": lry_fx,
            "ulx": ulx_pix, "uly": uly_pix, "lrx": lrx_pix, "lry": lry_pix,
        })
    i += 8

def viewport_of(r, res):
    # Mimic convertViewportRect with aspectScale=1, no origin/misalignment.
    # left = round(rect.left(ceil) * res)
    return (
        round(r["ulx"] * res),
        round(r["uly"] * res),
        round(r["lrx"] * res),
        round(r["lry"] * res),
    )

# For each abutting pair (Y-overlap, X right-edge of A == X left-edge of B), check
# whether at given res the converted pixel ranges actually abut.
def overlaps(a_lo, a_hi, b_lo, b_hi):
    return max(a_lo, b_lo) < min(a_hi, b_hi)

def find_pairs():
    out = []
    for a in texrects:
        for b in texrects:
            if a is b: continue
            # Right neighbor: B starts where A ends, both share Y range.
            if a["lrx"] == b["ulx"] and overlaps(a["uly"], a["lry"], b["uly"], b["lry"]):
                out.append(("right", a, b))
            # Bottom neighbor: B starts where A ends, both share X range.
            if a["lry"] == b["uly"] and overlaps(a["ulx"], a["lrx"], b["ulx"], b["lrx"]):
                out.append(("bottom", a, b))
    return out

pairs = find_pairs()
print(f"DL: {PATH}  TEXRECTs: {len(texrects)}  abutting pairs: {len(pairs)}\n")

for res in [1.0, 1.5, 2.0, 3.0]:
    print(f"=== resScale = {res:.2f} ===")
    gaps = overlaps_count = abuts = 0
    for kind, a, b in pairs:
        va = viewport_of(a, res)
        vb = viewport_of(b, res)
        if kind == "right":
            # Pixel coverage at top-left rule: A covers x in [va[0], va[2]); B covers [vb[0], vb[2])
            a_right_excl = va[2]
            b_left = vb[0]
            d = b_left - a_right_excl
        else:
            a_bot_excl = va[3]
            b_top = vb[1]
            d = b_top - a_bot_excl
        if d == 0: abuts += 1
        elif d > 0: gaps += 1
        else: overlaps_count += 1
    print(f"  abuts={abuts}  gaps>0={gaps}  overlaps<0={overlaps_count}")

# Also, dump first 5 pairs at res=1.5 to see what's happening
print("\n=== Sample pairs at resScale=1.5 ===")
for kind, a, b in pairs[:8]:
    va = viewport_of(a, 1.5)
    vb = viewport_of(b, 1.5)
    print(f"  {kind:6s}  A=+{a['off']:#06x} ({a['ulx']},{a['uly']})-({a['lrx']},{a['lry']}) -> vp{va}")
    print(f"          B=+{b['off']:#06x} ({b['ulx']},{b['uly']})-({b['lrx']},{b['lry']}) -> vp{vb}")
    if kind == "right":
        d = vb[0] - va[2]
        print(f"          right-edge of A = {va[2]} px; left-edge of B = {vb[0]} px; gap = {d} px")
    else:
        d = vb[1] - va[3]
        print(f"          bottom-edge of A = {va[3]} px; top-edge of B = {vb[1]} px; gap = {d} px")
