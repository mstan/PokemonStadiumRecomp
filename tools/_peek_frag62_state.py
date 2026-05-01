"""Peek runtime addresses for fragment62 BSS state vars."""
import json, socket
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

# fragment62 link=0x84300000 runtime=0x80123360 (per reg-frag log)
FRAG62_LINK = 0x84300000
FRAG62_RUN = 0x80123360
def to_run(link_addr):
    return FRAG62_RUN + (link_addr - FRAG62_LINK)

vars = [
    ("D_8438E440", 0x8438E440, 16),  # arg1 to func_8432D0D8
    ("D_8438E79C (mode)", 0x8438E79C, 4),  # switch state
    ("D_8438E7A0", 0x8438E7A0, 4),
    ("D_8438E7AC (exit ctr)", 0x8438E7AC, 4),
    ("D_8438E784 (geo node)", 0x8438E784, 4),
    ("D_8438E788", 0x8438E788, 4),
    ("D_8438E78C", 0x8438E78C, 4),
    ("D_8438E550 (handler arg)", 0x8438E550, 16),
]
for name, link, n in vars:
    rt = to_run(link)
    r = call({"cmd":"rdram_peek","addr":rt,"n":n})
    print(f"{name:30} link=0x{link:08X} run=0x{rt:08X} -> {r.get('hex') if r.get('ok') else r}")
