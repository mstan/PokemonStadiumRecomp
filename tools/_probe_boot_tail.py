"""Look at the END of the boot snapshot — last events recorded before
the buffer filled. That window is when the game thread was last active.
Also pulls a wide block of trace_recent at the same time to cross-reference.
"""
import socket, json
HOST, PORT = "127.0.0.1", 4371

def call(cmd, timeout=8):
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    s.sendall((cmd + "\n").encode()); s.settimeout(timeout)
    buf = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk: break
            buf += chunk
            if buf.endswith(b"\n"): break
    except socket.timeout: pass
    s.close()
    raw = buf.decode(errors="replace").strip()
    try: return json.loads(raw)
    except Exception: return {"_raw": raw}

# Tail of boot ring
total = call('{"cmd":"libultra_boot","start":0,"n":1}').get("count")
print(f"boot total = {total}")
TAIL = 800
start = max(0, total - TAIL)
r = call(f'{{"cmd":"libultra_boot","start":{start},"n":{TAIL}}}')
es = r.get("events", [])
print(f"\n=== last {len(es)} boot events (idx {start}..{start+len(es)-1}) ===")
for e in es:
    print(
        f"  i={e['i']:>5} ms={e['ms']:>6} {e['name']:<28} "
        f"a0=0x{e['a0']:08x} a1=0x{e['a1']:08x} a2=0x{e['a2']:08x} a3=0x{e['a3']:08x}"
    )
