"""Peek a sub-DL from RDRAM and decode it. Used to follow the G_DL
calls that dump_current_dl doesn't recurse into.
"""
import socket, json, sys, struct, collections

ADDR = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x8011D860
N    = int(sys.argv[2]) if len(sys.argv) > 2 else 2048
LABEL = sys.argv[3] if len(sys.argv) > 3 else f'subdl_{ADDR:08X}'

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

chunks = []
remaining = N
addr = ADDR
while remaining > 0:
    chunk = min(256, remaining)
    r = call({'cmd': 'rdram_peek', 'addr': addr, 'n': chunk})
    hp = r.get('hex', '')
    if not hp:
        print(f'no data at 0x{addr:08X}: {r}')
        break
    chunks.append(bytes.fromhex(hp))
    addr += chunk
    remaining -= chunk
data = b''.join(chunks)

OUT = f'F:/Projects/n64recomp/PokemonStadiumRecomp/build/dl_{LABEL}.bin'
with open(OUT, 'wb') as f:
    f.write(data)
print(f'wrote {len(data)} bytes to {OUT}')

# Quick decode: walk until G_ENDDL (0xDF) or end of buf
print(f'\n=== decode of 0x{ADDR:08X} ===')
i = 0
texrect_count = 0
vtx_count = 0
dl_calls = []
while i + 8 <= len(data):
    w0 = struct.unpack('>I', data[i:i+4])[0]
    w1 = struct.unpack('>I', data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    if op == 0xDF:
        print(f'  +{i:#06x}  G_ENDDL')
        break
    elif op == 0xDE:
        branch = (w0 >> 16) & 1
        dl_calls.append((i, w1, branch))
        print(f'  +{i:#06x}  G_DL    -> 0x{w1:08X}{" (branch)" if branch else ""}')
    elif op == 0x01:
        n = (w0 >> 12) & 0xFF
        v0 = ((w0 >> 1) & 0x7F)
        print(f'  +{i:#06x}  G_VTX   src=0x{w1:08X}  n={n}  v0={v0}')
        vtx_count += 1
    elif op == 0xE4:
        lrx = (w0 >> 12) & 0xFFF
        lry = w0 & 0xFFF
        ulx = (w1 >> 12) & 0xFFF
        uly = w1 & 0xFFF
        texrect_count += 1
        print(f'  +{i:#06x}  G_TEXRECT  scr=({ulx/4:6.1f},{uly/4:6.1f})-({lrx/4:6.1f},{lry/4:6.1f})')
    elif op == 0x05:
        print(f'  +{i:#06x}  G_TRI1')
    elif op == 0x06:
        print(f'  +{i:#06x}  G_TRI2')
    elif op == 0xFA:
        rgba = w1
        print(f'  +{i:#06x}  G_SETPRIMCOLOR  rgba=({(rgba>>24)&0xFF},{(rgba>>16)&0xFF},{(rgba>>8)&0xFF},{rgba&0xFF})')
    elif op == 0xFB:
        rgba = w1
        print(f'  +{i:#06x}  G_SETENVCOLOR   rgba=({(rgba>>24)&0xFF},{(rgba>>16)&0xFF},{(rgba>>8)&0xFF},{rgba&0xFF})')
    elif op == 0xFD:
        fmt = (w0 >> 21) & 7
        siz = (w0 >> 19) & 3
        w = (w0 & 0xFFF) + 1
        FMTS = ['RGBA','YUV','CI','IA','I']
        SIZS = ['4b','8b','16b','32b']
        print(f'  +{i:#06x}  G_SETTIMG  fmt={FMTS[fmt] if fmt<5 else "?"}/{SIZS[siz]}  w={w}  addr=0x{w1:08X}')
    i += 8
    if i >= len(data):
        print(f'  (reached end of {N}-byte window without G_ENDDL)')

print(f'\nsummary: {vtx_count} G_VTX, {texrect_count} G_TEXRECT, {len(dl_calls)} G_DL')
