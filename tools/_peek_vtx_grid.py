"""Peek a contiguous Vtx grid from RDRAM and dump it as Vtx records.
Usage: python tools/_peek_vtx_grid.py <addr_hex> <n_quads>
"""
import socket, json, sys, struct

ADDR = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x8011D460
NQUADS = int(sys.argv[2]) if len(sys.argv) > 2 else 16

def call(cmd, timeout=5):
    s = socket.create_connection(('127.0.0.1', 4371), timeout=timeout)
    s.sendall((json.dumps(cmd) + '\n').encode())
    buf = b''
    while True:
        try:
            c = s.recv(65536)
        except socket.timeout:
            break
        if not c: break
        buf += c
        if buf.endswith(b'\n'): break
    s.close()
    return json.loads(buf.decode().strip())

N = NQUADS * 0x40
chunks = []
addr = ADDR
remaining = N
while remaining > 0:
    c = min(256, remaining)
    r = call({'cmd':'rdram_peek','addr':addr,'n':c})
    chunks.append(bytes.fromhex(r['hex']))
    addr += c
    remaining -= c
data = b''.join(chunks)

print(f'=== Vtx grid at 0x{ADDR:08X}  ({NQUADS} quads, {N} bytes) ===')
for q in range(NQUADS):
    quad_off = q * 0x40
    print(f'\n  quad #{q:2d} (addr 0x{ADDR+quad_off:08X}):')
    for v in range(4):
        v_off = quad_off + v * 0x10
        x, y, z, fl, s, t, cr, cg, cb, ca = struct.unpack('>hhhhhhBBBB', data[v_off:v_off+0x10])
        print(f'    V{v}: xyz=({x:5d},{y:5d},{z:5d}) flag={fl:#06x} st=({s:5d},{t:5d}) rgba=({cr:3d},{cg:3d},{cb:3d},{ca:3d})')
