"""Flip strip 7's t-coords (top<->bottom) on quad #7 (0x8011D620) and
quad #15 (0x8011D820). Then the navy border (img rows 140-150) maps to the
BOTTOM of the quad and the edge (rows 150-152) to the TOP.

Observe where the transparent (tan) area lands:
  - stays at BOTTOM of quad  -> positional bug (bottom of short quad not filled)
  - moves to TOP of quad     -> content bug (high texel rows sample transparent)

Pass 'restore' to put t back (top=4480, bottom=4864).
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
TOP = 4480   # row 140
BOT = 4864   # row 152
# vertex order V0(top-left) V1(bot-left) V2(bot-right) V3(top-right)
# normal: V0,V3 t=TOP ; V1,V2 t=BOT
# flipped: V0,V3 t=BOT ; V1,V2 t=TOP
if restore:
    tvals = {0: TOP, 1: BOT, 2: BOT, 3: TOP}
else:
    tvals = {0: BOT, 1: TOP, 2: TOP, 3: BOT}

for quad_base in (0x8011D620, 0x8011D820):
    for v, tv in tvals.items():
        addr = quad_base + v * 0x10 + 10  # t is at vertex+10
        r = call({'cmd': 'rdram_poke', 'addr': addr, 'hex': struct.pack('>H', tv).hex()})
        print(f'poke t @0x{addr:08X} = {tv} -> {r["ok"]}')
print('flipped' if not restore else 'restored')
