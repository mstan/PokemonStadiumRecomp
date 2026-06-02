"""Query the always-on vtx_ring and isolate triangles belonging to the
central POKeMON STADIUM panel, to see whether the short bottom-border
quad reaches the screen and at what Y.

Usage: python tools/_panel_tris.py [count] [xlo xhi ylo yhi]
"""
import socket, json, sys

def call(cmd, t=5):
    s = socket.create_connection(('127.0.0.1', 4371), timeout=t)
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

count = int(sys.argv[1]) if len(sys.argv) > 1 else 512
d = call({'cmd': 'vtx_ring', 'count': count})
es = d['entries']
xs = [v for e in es for v in (e['v0'][0], e['v1'][0], e['v2'][0])]
ys = [v for e in es for v in (e['v0'][1], e['v1'][1], e['v2'][1])]
print(f"n_tris={len(es)}  write_idx={d['write_idx']}  start_seq={d['start_seq']}")
print(f"x range [{min(xs):.1f}, {max(xs):.1f}]   y range [{min(ys):.1f}, {max(ys):.1f}]")

# Optional region filter
if len(sys.argv) >= 6:
    xlo, xhi, ylo, yhi = (float(sys.argv[i]) for i in range(2, 6))
    print(f"\n=== triangles with all verts in x[{xlo},{xhi}] y[{ylo},{yhi}] ===")
    for e in es:
        vx = [e['v0'][0], e['v1'][0], e['v2'][0]]
        vy = [e['v0'][1], e['v1'][1], e['v2'][1]]
        if all(xlo <= x <= xhi for x in vx) and all(ylo <= y <= yhi for y in vy):
            print(f"  seq {e['seq']:>7}  "
                  f"({e['v0'][0]:.1f},{e['v0'][1]:.1f}) "
                  f"({e['v1'][0]:.1f},{e['v1'][1]:.1f}) "
                  f"({e['v2'][0]:.1f},{e['v2'][1]:.1f})  "
                  f"yspan=[{min(vy):.1f},{max(vy):.1f}]")
