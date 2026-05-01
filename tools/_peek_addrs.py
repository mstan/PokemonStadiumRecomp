"""Peek arbitrary RDRAM addresses (decimal hex pairs after script name)."""
import json, socket, sys

def call(cmd, t=2.0):
    s=socket.create_connection(("127.0.0.1",4371),timeout=t)
    s.sendall((json.dumps(cmd)+"\n").encode())
    s.settimeout(t); buf=b""
    while True:
        try: c=s.recv(65536)
        except: break
        if not c: break
        buf+=c
        if buf.endswith(b"\n"): break
    s.close()
    return json.loads(buf.decode().strip())

# Default: a list of suspect addrs
addrs = [
    ("D_800A7464", 0x800A7464, 4),
    ("*D_800A7464 first 16", None, None),  # filled after first read
    ("gPlayer1Controller (vram 0x800ABE10)", 0x800ABE10, 16),
]
results = {}
for name, addr, n in addrs[:1]:
    r = call({"cmd":"rdram_peek","addr":addr,"n":n})
    print(f"{name} @ 0x{addr:08X}: {r.get('hex')}")
    if r.get("ok"):
        # parse big-endian u32
        h = r["hex"]
        ptr = int(h, 16)
        print(f"  -> ptr = 0x{ptr:08X}")
        if ptr >= 0x80000000 and ptr < 0x80800000:
            r2 = call({"cmd":"rdram_peek","addr":ptr,"n":48})
            print(f"  *ptr [48 bytes]: {r2.get('hex')}")
for name, addr, n in addrs[2:]:
    r = call({"cmd":"rdram_peek","addr":addr,"n":n})
    print(f"{name} @ 0x{addr:08X}: {r.get('hex') if r.get('ok') else r}")
