"""Read source bytes for ALL the panel-fill strips at +0x7150, +0x71f8, ...
Each is 8x8 IA/8b, sampling column 0 with dsdx=0 dtdy=1. Find the
strip's TIMG, peek bytes, decode I/A.
"""
import socket, json, struct, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_dl_gpk_now.bin"

def ask(cmd):
    s = socket.socket(); s.settimeout(5); s.connect(('127.0.0.1',4371))
    s.sendall((json.dumps(cmd)+'\n').encode())
    return json.loads(s.recv(32768).decode())

seg = ask({'cmd':'rt64_segments'})
segments = seg.get('segments', [0]*16)

with open(PATH, "rb") as f:
    data = f.read()

# Walk DL, track current TIMG, find every TEXRECT with dsdx=0 dtdy=1 (horizontal strip)
# in y range [120, 290] and report its TIMG + first 8 bytes (column 0 of a row)
timg = None
strips = []
i = 0
while i+8 <= len(data):
    w0 = struct.unpack(">I", data[i:i+4])[0]
    w1 = struct.unpack(">I", data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    if op == 0xFD:
        timg = ((w0>>21)&7, (w0>>19)&3, (w0&0xFFF)+1, w1)
    elif op == 0xE4:
        lrx = (w0 >> 12) & 0xFFF
        lry = w0 & 0xFFF
        ulx = (w1 >> 12) & 0xFFF
        uly = w1 & 0xFFF
        m1 = struct.unpack(">I", data[i+20:i+24])[0]  if i+24<=len(data) else 0
        dsdx = (m1 >> 16) & 0xFFFF
        dtdy = m1 & 0xFFFF
        scr_y = uly/4.0
        if dsdx == 0 and dtdy == 0x0400 and 120 <= scr_y <= 290:
            sw = (lrx - ulx) / 4.0
            sh = (lry - uly) / 4.0
            if sw > 100 and timg:
                strips.append({"off":i, "scr":(ulx/4, uly/4, sw, sh), "timg":timg})
    i += 8

print(f"Found {len(strips)} large horizontal strips in y=120..290:\n")

FMT = {0:"RGBA",1:"YUV",2:"CI",3:"IA",4:"I"}
SIZ = {0:"4b",1:"8b",2:"16b",3:"32b"}

# Group by TIMG addr (multiple strips often share)
seen_timg = {}
for s in strips:
    fmt, siz, w, addr = s["timg"]
    key = addr
    if key not in seen_timg:
        seg_byte = (addr >> 24) & 0xFF
        if 0 <= seg_byte < 16:
            base = segments[seg_byte]
            rdram = (base + (addr & 0xffffff)) | 0x80000000
            r = ask({'cmd':'rdram_peek','addr':rdram,'n':64})
            seen_timg[key] = (rdram, bytes.fromhex(r.get('hex','')))

for s in strips:
    fmt, siz, w, addr = s["timg"]
    rdram, raw = seen_timg.get(addr, (0, b''))
    sx, sy, sw, sh = s["scr"]
    print(f"+{s['off']:#06x}  scr=({sx:5.1f},{sy:5.1f}) {sw:5.1f}x{sh:4.1f}  TIMG=0x{addr:08x} {FMT.get(fmt,'?')}/{SIZ.get(siz,'?')}  -> RDRAM=0x{rdram:08x}")
    if raw:
        # Column 0 (S=0) for each of 8 rows: byte 0 of each 8-byte row
        col0_bytes = [raw[r*8] for r in range(8)]
        print(f"  col 0 IA/8b bytes (rows 0..7): {' '.join(f'{b:02x}' for b in col0_bytes)}")
        col0_alphas = ''.join(f'{b & 0xF:X}' for b in col0_bytes)
        col0_intensities = ''.join(f'{(b>>4) & 0xF:X}' for b in col0_bytes)
        print(f"  col 0 intensity:  {col0_intensities}     alpha: {col0_alphas}")
