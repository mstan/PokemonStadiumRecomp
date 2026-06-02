"""Experiment: stretch the STADIUM panel bottom strip (quad #7 left @0x8011D620
and quad #15 right @0x8011D820) downward by moving their bottom-edge vertices
(V1,V2) y from -76 to a large negative, to test whether RT64 rasterizes them.

Pass 'restore' to put y back to -76.
"""
import socket, json, sys, struct

HOST = ('127.0.0.1', 4371)

def call(cmd):
    s = socket.create_connection(HOST, timeout=5)
    s.sendall((json.dumps(cmd) + '\n').encode())
    buf = b''
    while not buf.endswith(b'\n'):
        c = s.recv(65536)
        if not c:
            break
        buf += c
    s.close()
    return json.loads(buf.decode().strip())

restore = len(sys.argv) > 1 and sys.argv[1] == 'restore'
newy = -76 if restore else -160
yb = struct.pack('>h', newy).hex()

# V1.y and V2.y of each quad. quad base + 0x10 (V1) + 2, quad base + 0x20 (V2) + 2
for quad_base in (0x8011D620, 0x8011D820):
    for v in (1, 2):
        addr = quad_base + v * 0x10 + 2
        r = call({'cmd': 'rdram_poke', 'addr': addr, 'hex': yb})
        print(f'poke 0x{addr:08X} = {yb} -> {r}')
print('newy', newy)
