"""Peek N bytes from live RDRAM and write raw to a .bin file.
Usage: python tools/_peek_bin.py <addr> <nbytes> <out.bin>
"""
import socket, json, sys

HOST = ('127.0.0.1', 4371)

def peek(addr, n):
    s = socket.create_connection(HOST, timeout=5)
    s.sendall((json.dumps({'cmd': 'rdram_peek', 'addr': addr, 'n': n}) + '\n').encode())
    buf = b''
    while not buf.endswith(b'\n'):
        c = s.recv(65536)
        if not c:
            break
        buf += c
    s.close()
    return json.loads(buf.decode().strip())

addr = int(sys.argv[1], 0)
n = int(sys.argv[2])
out = sys.argv[3]
data = b''
got = 0
while got < n:
    chunk = min(256, n - got)
    r = peek(addr + got, chunk)
    if not r.get('ok'):
        print('ERR', r); sys.exit(1)
    data += bytes.fromhex(r['hex'])
    got += chunk
with open(out, 'wb') as f:
    f.write(data)
print(f'wrote {len(data)} bytes from 0x{addr:08X} to {out}')
