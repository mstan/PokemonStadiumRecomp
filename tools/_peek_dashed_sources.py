"""Peek RDRAM source bytes for the Game Pak Check dashed-strip
textures, decode as I/4b nibbles to see whether the texture
itself is dashed (RT64 correctly rendering bad data) or solid
(RT64 mis-decoding good data)."""
import socket, json

def ask(cmd):
    s = socket.socket(); s.settimeout(5); s.connect(('127.0.0.1',4371))
    s.sendall((json.dumps(cmd)+'\n').encode())
    return json.loads(s.recv(32768).decode())

seg = ask({'cmd':'rt64_segments'})
segments = seg.get('segments', [0]*16) if seg.get('ok') else [0]*16
seg3 = segments[3]
print(f'seg[3] = 0x{seg3:08x}')

addrs = [0x0300f240, 0x0300f280, 0x0300f2c0, 0x0300f300, 0x0300f340]

for off in addrs:
    rdram_addr = (seg3 + (off & 0xffffff)) | 0x80000000
    r = ask({'cmd':'rdram_peek','addr':rdram_addr,'n':64})
    hex_payload = r.get('hex','')
    if not hex_payload:
        print(f'\n0x{off:08x}: NO DATA (rdram=0x{rdram_addr:08x})')
        continue
    raw = bytes.fromhex(hex_payload)
    print(f'\n=== TIMG 0x{off:08x} -> RDRAM 0x{rdram_addr:08x} (64 bytes) ===')
    print('Raw bytes (hex), 8 bytes per row (= 1 SAMPLE-row of 16 I/4b nibbles):')
    for row in range(8):
        bs = raw[row*8 : row*8+8]
        print(f'  row {row}: {" ".join(f"{b:02x}" for b in bs)}')
    print('I/4b nibble grid (16 nibbles per row x 8 rows):')
    for row in range(8):
        line = ''
        for byte_off in range(8):
            b = raw[row*8 + byte_off]
            line += f'{(b>>4)&0xF:X}{b&0xF:X}'
        print(f'  row {row}:  {line}')
