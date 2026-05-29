#!/usr/bin/env python3
# Reader for the debug-server rdram_peek command. Dumps N bytes at a
# vaddr (XOR-3 / N64-logical byte order handled server-side) as a hex
# dump plus a quick entropy summary (distinct byte values, zero count,
# 16-bit-zero pixel count) to tell a real texture (structured / many
# transparent pixels) from garbage/compressed noise (high entropy).
#
# Usage: python tools/_peek.py <addr> [nbytes]   addr accepts 0x... hex
import socket, json, sys
from collections import Counter

HOST = ('127.0.0.1', 4371)


def peek(addr, n):
    s = socket.create_connection(HOST, timeout=4)
    s.sendall((json.dumps({'cmd': 'rdram_peek', 'addr': addr, 'n': n}) + '\n').encode())
    buf = b''
    while not buf.endswith(b'\n'):
        c = s.recv(65536)
        if not c:
            break
        buf += c
    s.close()
    return json.loads(buf.decode().strip())


def main():
    addr = int(sys.argv[1], 0)
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 256
    out = ''
    got = 0
    while got < n:
        chunk = min(256, n - got)
        r = peek(addr + got, chunk)
        if not r.get('ok'):
            print('ERR', r)
            return
        out += r['hex']
        got += chunk
    print(f'addr={hex(addr)} n={got}')
    for i in range(0, len(out), 64):
        row = out[i:i + 64]
        cells = ' '.join(row[j:j + 2] for j in range(0, len(row), 2))
        print(f'  {addr + i // 2:08x}: {cells}')
    bs = [int(out[i:i + 2], 16) for i in range(0, len(out), 2)]
    c = Counter(bs)
    # 16-bit pixels (RGBA16): count fully-zero pixels (transparent bg)
    px = [out[i:i + 4] for i in range(0, len(out), 4)]
    zero_px = sum(1 for p in px if p == '0000')
    pc = Counter(px)
    print(f'  -- distinct byte values={len(c)}/256  zero bytes={c.get(0,0)}/{len(bs)}'
          f'  zero(0x0000) pixels={zero_px}/{len(px)}'
          f'  most-common pixel={pc.most_common(1)[0] if px else None}'
          f'  distinct pixels={len(pc)}/{len(px)}')


if __name__ == '__main__':
    main()
