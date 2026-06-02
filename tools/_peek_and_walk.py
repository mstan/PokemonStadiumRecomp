"""Peek RDRAM at a vaddr and walk it as DL commands."""
import socket, json, sys, struct


def call(cmd, t=5):
    s = socket.create_connection(('127.0.0.1', 4371), timeout=t)
    s.sendall((json.dumps(cmd) + '\n').encode())
    buf = b''
    s.settimeout(t)
    while True:
        try:
            c = s.recv(65536)
        except socket.timeout:
            break
        if not c:
            break
        buf += c
        if buf.endswith(b'\n'):
            break
    s.close()
    return json.loads(buf.decode().strip())


def peek(addr, n):
    out = b''
    while n > 0:
        c = min(256, n)
        r = call({'cmd': 'rdram_peek', 'addr': addr, 'n': c})
        out += bytes.fromhex(r['hex'])
        addr += c
        n -= c
    return out


ADDR = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x80208F18
NBYTES = int(sys.argv[2]) if len(sys.argv) > 2 else 800

data = peek(ADDR, NBYTES)
print(f'walking {NBYTES} bytes from 0x{ADDR:08X}:')
for i in range(0, NBYTES - 8, 8):
    w0, w1 = struct.unpack('>II', data[i:i+8])
    op = (w0 >> 24) & 0xFF
    name = {
        0x00: 'G_NOOP',
        0xDE: 'G_DL',
        0xDF: 'G_ENDDL',
        0xD7: 'G_TEXTURE',
        0xD9: 'G_SETOTHERMODE_H',
        0xDA: 'G_MTX',
        0xDB: 'G_MOVEMEM',
        0xDC: 'G_MOVEWORD',
        0xE3: 'G_SETOTHERMODE_L*',
        0xE7: 'G_RDPPIPESYNC',
        0xF1: 'G_TRI2',
        0xF5: 'G_SETTILE',
        0xFC: 'G_SETCOMBINE',
        0xFD: 'G_SETTIMG',
    }.get(op, f'op_0x{op:02X}')
    flag = ''
    suspect = ''
    if op == 0xDE:
        push = (w0 >> 16) & 0xFF
        flag = f' push={push} target=0x{w1:08X}'
        # Sanity: valid G_DL has low 24 bits of w0 ALL zero, segment byte <= 15
        if (w0 & 0xFF00FFFF) != 0xDE000000:
            suspect = '  *** GARBAGE w0 (junk bits in G_DL command word) ***'
    elif op == 0xDF:
        # G_ENDDL: w0 must be 0xDF000000, w1 typically 0
        if w0 != 0xDF000000 or w1 != 0:
            suspect = '  *** GARBAGE ENDDL (non-zero junk bits) ***'
    print(f'  +{i:#06x} (vaddr=0x{ADDR+i:08X}) w0=0x{w0:08X} w1=0x{w1:08X}  op=0x{op:02X} {name}{flag}{suspect}')
    if op == 0xDF and not suspect:
        print('   *** clean G_ENDDL — walk would stop here ***')
        break
