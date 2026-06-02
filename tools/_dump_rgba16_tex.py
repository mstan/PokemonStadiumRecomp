"""Dump an RGBA16 texture from live RDRAM and save as PNG (with an alpha
visualization alongside). RGBA16 = 5551 (r5 g5 b5 a1), big-endian per texel.

Usage: python tools/_dump_rgba16_tex.py <addr> <width> <height> <out.png>
"""
import socket, json, sys
from PIL import Image

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
W = int(sys.argv[2]); H = int(sys.argv[3]); out = sys.argv[4]
nbytes = W * H * 2
data = b''
got = 0
while got < nbytes:
    chunk = min(256, nbytes - got)
    r = peek(addr + got, chunk)
    if not r.get('ok'):
        print('ERR', r); sys.exit(1)
    b = bytes.fromhex(r['hex'])
    if not b:
        print(f'short read at offset {got}'); break
    data += b
    got += len(b)
if len(data) < nbytes:
    print(f'WARNING: got {len(data)} of {nbytes} bytes; truncating height')
    H = len(data) // (W * 2)

img = Image.new('RGBA', (W, H))
alpha_img = Image.new('RGB', (W, H))
px = img.load(); ap = alpha_img.load()
for y in range(H):
    for x in range(W):
        o = (y * W + x) * 2
        texel = (data[o] << 8) | data[o+1]
        r5 = (texel >> 11) & 0x1F
        g5 = (texel >> 6) & 0x1F
        b5 = (texel >> 1) & 0x1F
        a1 = texel & 0x1
        R = (r5 << 3) | (r5 >> 2)
        G = (g5 << 3) | (g5 >> 2)
        B = (b5 << 3) | (b5 >> 2)
        A = 255 if a1 else 0
        px[x, y] = (R, G, B, A)
        ap[x, y] = (255, 255, 255) if a1 else (0, 0, 0)
# composite over magenta so transparent texels are obvious
bg = Image.new('RGBA', (W, H), (255, 0, 255, 255))
comp = Image.alpha_composite(bg, img).convert('RGB')
scale = 4
comp.resize((W*scale, H*scale), Image.NEAREST).save(out)
alpha_img.resize((W*scale, H*scale), Image.NEAREST).save(out.replace('.png', '_alpha.png'))
print(f'wrote {out} ({W}x{H}) and alpha map; transparent texels shown as magenta')
