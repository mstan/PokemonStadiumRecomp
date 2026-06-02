"""Scan main 8MB RDRAM for the well-known frag57 Vtx-grid header.
If found, fragment 57's data lives inside the standard RDRAM region
and rdram_peek/poke can reach it.

Pattern: first quad V0 at offset 0x7008 is xyz=(-64,0,0), st=(0,0),
rgba=FFFFFFFF — i.e. ffc0 0000 0000 0000 0000 0000 ffff ffff
"""
import socket, json, struct

def call(cmd):
    s = socket.create_connection(('127.0.0.1',4371), timeout=5)
    s.sendall((json.dumps(cmd)+'\n').encode())
    buf = b''
    while True:
        try: c = s.recv(65536)
        except socket.timeout: break
        if not c: break
        buf += c
        if buf.endswith(b'\n'): break
    s.close()
    return json.loads(buf.decode().strip())

# rdram_scan_u32 takes a single u32 value. Let's scan for the first
# word of the Vtx record (ffc0 0000 = signed -64,0 packed as two int16).
# Big-endian: 0xFFC00000.
r = call({'cmd':'rdram_scan_u32', 'value': 0xFFC00000, 'limit': 256})
hits = r.get('hits', r.get('addrs', []))
print(f'scan for 0xFFC00000: {len(hits)} hits')
for h in hits[:32]:
    print(f'  0x{h:08X}')
