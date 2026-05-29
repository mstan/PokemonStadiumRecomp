#!/usr/bin/env python3
# Reader for the debug-server rdram_scan_u32 command. Finds every
# word-aligned RDRAM location holding a given u32 (N64 big-endian view).
# Use to locate pointer-holders: if a texture/DL sources from addr X,
# scan for X to find the table/struct entry that supplied it.
#
# Usage: python tools/_scan.py <value> [limit]   value accepts 0x... hex
import socket, json, sys

HOST = ('127.0.0.1', 4371)


def scan(value, limit):
    s = socket.create_connection(HOST, timeout=8)
    s.sendall((json.dumps({'cmd': 'rdram_scan_u32', 'value': value, 'limit': limit}) + '\n').encode())
    buf = b''
    while not buf.endswith(b'\n'):
        c = s.recv(65536)
        if not c:
            break
        buf += c
    s.close()
    return json.loads(buf.decode().strip())


def main():
    value = int(sys.argv[1], 0)
    limit = int(sys.argv[2]) if len(sys.argv) > 2 else 256
    r = scan(value, limit)
    if not r.get('ok'):
        print('ERR', r)
        return
    hits = r.get('hits', [])
    print(f'value={hex(value)}  hits={len(hits)}  truncated={r.get("truncated")}')
    for h in hits:
        print(f'  holder=0x{h & 0xffffffff:08x}')


if __name__ == '__main__':
    main()
