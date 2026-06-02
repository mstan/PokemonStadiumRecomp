"""Capture a full frame's worth of vtx_ring triangles, isolate the STADIUM
panel's vertical quad stack (central x-band), and print each quad-band's
screen Y-span sorted top->bottom. Reveals whether the bottom strip (quad #7)
reaches the screen and at what height.

Usage: python tools/_capture_panel.py [xlo xhi ylo yhi]
"""
import socket, json, sys

HOST = ('127.0.0.1', 4371)

def call(cmd, t=5):
    s = socket.create_connection(HOST, timeout=t)
    s.sendall((json.dumps(cmd) + '\n').encode())
    buf = b''
    s.settimeout(t)
    while True:
        try:
            c = s.recv(65536)
        except socket.timeout:
            break
        if not c:
            break
        buf += c
        if buf.endswith(b'\n'):
            break
    s.close()
    return json.loads(buf.decode().strip())

# region of interest (internal render coords ~640x480)
xlo, xhi, ylo, yhi = (200.0, 420.0, 100.0, 300.0)
if len(sys.argv) >= 5:
    xlo, xhi, ylo, yhi = (float(sys.argv[i]) for i in range(1, 5))

first = call({'cmd': 'vtx_ring', 'count': 1})
wi = first['write_idx']
# pull ~4000 most-recent tris in 512 chunks
tris = []
seqs_seen = set()
start = max(1, wi - 4000)
seq = start
while seq <= wi:
    r = call({'cmd': 'vtx_ring', 'count': 512, 'start_seq': seq})
    for e in r['entries']:
        if e['seq'] in seqs_seen:
            continue
        seqs_seen.add(e['seq'])
        tris.append(e)
    seq += 512

print(f"captured {len(tris)} tris (write_idx={wi})")
# filter to ROI: all 3 verts inside the box
roi = []
for e in tris:
    vs = [e['v0'], e['v1'], e['v2']]
    if all(xlo <= v[0] <= xhi and ylo <= v[1] <= yhi for v in vs):
        ys = [v[1] for v in vs]
        xs = [v[0] for v in vs]
        roi.append((min(ys), max(ys), min(xs), max(xs), e['seq']))
print(f"ROI tris (x[{xlo},{xhi}] y[{ylo},{yhi}]): {len(roi)}")
# dedupe by rounded geometry, count occurrences
from collections import Counter
geoms = Counter()
for ymin, ymax, xmin, xmax, sq in roi:
    key = (round(ymin, 1), round(ymax, 1), round(xmin, 1), round(xmax, 1))
    geoms[key] += 1
print(f"distinct quad-triangle geometries: {len(geoms)} (sorted by ymin then xmin)")
for (ymin, ymax, xmin, xmax), cnt in sorted(geoms.items()):
    print(f"  y[{ymin:6.1f},{ymax:6.1f}] h={ymax-ymin:5.1f}  x[{xmin:6.1f},{xmax:6.1f}] w={xmax-xmin:5.1f}  (x{cnt})")
