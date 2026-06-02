"""Decode an OSMesgQueue at a given vaddr."""
import socket, json, struct, sys

ADDR = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x800846A4


def call(cmd, t=4):
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


d = call({'cmd': 'rdram_peek', 'addr': ADDR, 'n': 32})
hx = d.get('hex', '')
print(f'raw @ 0x{ADDR:08X}: {hx}')
data = bytes.fromhex(hx)
mtq, fullq, valid, first, msgCount, msgPtr = struct.unpack('>IIiiII', data[:24])
print(f'  mtqueue   = 0x{mtq:08X}   (head of threads waiting to recv)')
print(f'  fullqueue = 0x{fullq:08X}   (head of threads waiting on full)')
print(f'  validCount = {valid}')
print(f'  first      = {first}')
print(f'  msgCount   = {msgCount}')
print(f'  msg ptr    = 0x{msgPtr:08X}')
