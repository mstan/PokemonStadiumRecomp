"""Poke STADIUM bottom strip (quad #7 @0x8011D620, quad #15 @0x8011D820) to a
given height by setting the bottom-edge vertices' (V1,V2) y. Top edge stays -64.
Usage: python tools/_poke_height.py <bottom_y>   (e.g. -76 native, -84 for 20u, -104 for 40u)
"""
import socket, json, sys, struct
HOST = ('127.0.0.1', 4371)
def call(c):
    s = socket.create_connection(HOST, timeout=5)
    s.sendall((json.dumps(c) + '\n').encode())
    b = b''
    while not b.endswith(b'\n'):
        d = s.recv(65536)
        if not d: break
        b += d
    s.close(); return json.loads(b.decode().strip())
by = int(sys.argv[1])
yb = struct.pack('>h', by).hex()
for base in (0x8011D620, 0x8011D820):
    for v in (1, 2):
        call({'cmd': 'rdram_poke', 'addr': base + v * 0x10 + 2, 'hex': yb})
print(f'bottom y = {by} (height {64 + (-by) if by < 0 else 64 - by} units from top -64)')
