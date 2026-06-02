"""Quick capture of current DL + RDRAM segment table, tagged with a label.
Usage: python tools/_capture_now.py <label>
"""
import socket, json, sys, os

LABEL = sys.argv[1] if len(sys.argv) > 1 else 'now'
OUT_DL = f'F:/Projects/n64recomp/PokemonStadiumRecomp/build/dl_{LABEL}.bin'

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

st = call({'cmd':'status'})
print(f"status: frame={st['frame']} send_dl={st['send_dl']}")

seg = call({'cmd':'rt64_segments'})
if seg.get('ok'):
    segs = seg.get('segments', [])
    print(f"segments: {' '.join(f'[{i}]={s:#010x}' for i,s in enumerate(segs) if s != 0)}")

r = call({'cmd':'dump_current_dl', 'path': OUT_DL})
print(f"dump_current_dl: {r}")
