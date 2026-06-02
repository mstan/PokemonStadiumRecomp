"""Snapshot diagnostics for a white-screen softlock event.

Pulls:
  - gCurrentGameState (vaddr 0x80075668)
  - Recent trace ring (if available)
  - interesting_fns counters
  - send_dl rate + game-thread liveness probes

Run this against a stuck runner to capture state without affecting it.
"""
import socket, json, sys

HOST = ('127.0.0.1', 4371)


def call(cmd, t=4):
    try:
        s = socket.create_connection(HOST, timeout=t)
    except OSError:
        return None
    try:
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
    finally:
        s.close()
    try:
        return json.loads(buf.decode().strip())
    except Exception:
        return {'raw': buf.decode(errors='replace')}


def peek_u32(vaddr):
    r = call({'cmd': 'rdram_peek', 'addr': vaddr, 'n': 4})
    if r is None or 'hex' not in r:
        return None, r
    hx = r['hex']
    if len(hx) != 8:
        return None, r
    return int(hx, 16), hx


print('=== Softlock diag ===')

st = call({'cmd': 'status'})
print(f'\nstatus: {st}')

# gCurrentGameState
val, hx = peek_u32(0x80075668)
print(f'\ngCurrentGameState @ 0x80075668: 0x{val:08X}' if val is not None else f'gCurrentGameState read failed: {hx}')

# sMemPool.available at +0x1C of 0x800A6070
val, _ = peek_u32(0x800A6070 + 0x1C)
print(f'sMemPool.available @ 0x800A608C: 0x{val:08X}' if val is not None else 'sMemPool read failed')

# interesting_fns counters
r = call({'cmd': 'interesting_fns'})
print(f'\ninteresting_fns: {r}'[:1200])

# rt64_segments
r = call({'cmd': 'rt64_segments'})
print(f'\nrt64_segments: {r}')

# dump_threads
r = call({'cmd': 'dump_threads'})
print(f'\ndump_threads:\n{r}'[:2000] if r else 'dump_threads failed')
