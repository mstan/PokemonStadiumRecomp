"""Read source bytes for the IA/16b->IA/8b texture used at +0x7150
(panel-fill strip). Decode each byte as IA/8b (4-bit I, 4-bit A).
"""
import socket, json

def ask(cmd):
    s = socket.socket(); s.settimeout(5); s.connect(('127.0.0.1',4371))
    s.sendall((json.dumps(cmd)+'\n').encode())
    return json.loads(s.recv(32768).decode())

seg = ask({'cmd':'rt64_segments'})
segments = seg.get('segments', [0]*16)
print(f'segments: seg[2]=0x{segments[2]:08x}')

# TIMG 0x02000c00 -> seg[2] + 0x000c00
addr = (segments[2] + 0x000c00) | 0x80000000
r = ask({'cmd':'rdram_peek','addr':addr,'n':64})
hex_payload = r.get('hex','')
raw = bytes.fromhex(hex_payload)
print(f'\n=== TIMG 0x02000c00 -> RDRAM 0x{addr:08x} (64 bytes, 8x8 IA/8b) ===')
print('Raw bytes (hex), 8 bytes per row (= 8 IA/8b texels per row):')
for row in range(8):
    bs = raw[row*8 : row*8+8]
    print(f'  row {row}: {" ".join(f"{b:02x}" for b in bs)}')

print('\nDecode as IA/8b (4-bit Intensity, 4-bit Alpha) per texel:')
print('  Format: I=intensity (0-F)  A=alpha (0-F)')
for row in range(8):
    parts = []
    for col in range(8):
        b = raw[row*8 + col]
        I = (b >> 4) & 0xF
        A = b & 0xF
        parts.append(f'{I:X}{A:X}')
    print(f'  row {row}:  {" ".join(parts)}')

print('\nAlpha-only grid:')
for row in range(8):
    line = ''
    for col in range(8):
        b = raw[row*8 + col]
        A = b & 0xF
        line += f'{A:X}'
    print(f'  row {row}:  {line}')
