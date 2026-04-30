"""Grab live trace_recent + a memory peek at 0x8018A1B0."""
import socket, json, sys

def call(c, t=3):
    s = socket.create_connection(('127.0.0.1', 4371), timeout=t)
    s.sendall((c + '\n').encode())
    buf = b''
    while not buf.endswith(b'\n'):
        x = s.recv(65536)
        if not x:
            break
        buf += x
    s.close()
    return json.loads(buf.decode().strip())

r = call('trace_recent')
events = r.get('events', [])
print(f'trace_ring last {len(events)} events:')
for e in events[-50:]:
    print(' ', e)

# Peek at 0x8018A1B0 — should still be corrupted if AREA_SELECT is active
r = call(json.dumps({'cmd': 'rdram_peek', 'addr': 0x8018A1B0, 'n': 64}))
print('\n0x8018A1B0..0x8018A1F0 (live, hex):')
print(r.get('hex', '?'))

# Also peek at 0x8018A1B0 - 0x10 to see context before
r = call(json.dumps({'cmd': 'rdram_peek', 'addr': 0x8018A1A0, 'n': 16}))
print('\n0x8018A1A0..0x8018A1B0 (just before SB):')
print(r.get('hex', '?'))
