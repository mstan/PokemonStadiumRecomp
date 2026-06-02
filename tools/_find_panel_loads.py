"""Capture tmem_ring and isolate the STADIUM panel's RGBA16 loadTile ops
(the 16 strips). Print src_addr, rows, width, hash so we can see whether the
bottom strip (src ~0x4D2A20) is loaded with the right rows/content.
"""
import socket, json

HOST = ('127.0.0.1', 4371)
IMG_BASE = 0x4C6D80
WIDTH = 172

def call(cmd, t=6):
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

first = call({'cmd': 'tmem_ring', 'count': 1})
wi = first['write_idx']
seen = {}
seq = max(1, wi - 4000)
while seq <= wi:
    r = call({'cmd': 'tmem_ring', 'count': 2048, 'start_seq': seq})
    for e in r['entries']:
        seen[e['seq']] = e
    seq += 2048

# RGBA16 tile loads (fmt 0 siz 2) that look like panel strips (width 100 or 72)
panel = []
for sq, e in seen.items():
    if e['op'] == 'tile' and e['fmt'] == 0 and e['siz'] == 2:
        panel.append(e)
# dedupe by (src_addr, rows, width)
uniq = {}
for e in panel:
    key = (e['src_addr'], e['rows'], e['width'])
    uniq[key] = e
print(f"distinct RGBA16 tile loads: {len(uniq)}")
for (src, rows, width), e in sorted(uniq.items()):
    rel = src - IMG_BASE
    row0 = rel // (WIDTH * 2) if rel >= 0 else None
    col0 = (rel % (WIDTH * 2)) // 2 if rel >= 0 else None
    print(f"  src=0x{src:08X} (img+{rel:6d} -> row {row0}, col {col0})  rows={rows} width={width} hash={e['hash']}")
