"""Deeper probe: full trace ring + boot prefix + thread states.

Locates each OSThread struct via osCreateThread events in libultra_boot,
then reads OSThread.state/.pri/.context.pc via rdram_peek so we can see
which thread is blocked and where.
"""
import socket, json
HOST, PORT = "127.0.0.1", 4371

def call(cmd: str, timeout=5):
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    s.sendall((cmd + "\n").encode())
    s.settimeout(timeout)
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

# Full 512-entry trace ring — look for unique names (the steady-state cycle
# repeats; anything that appears once is the last "interesting" activity).
print("=== trace_recent n=512 — uniques in chronological order ===")
tr = call('{"cmd":"trace_recent","n":512}')
es = tr.get("entries", [])
print(f"write_idx={tr.get('write_idx')} count={len(es)}")
seen = []
for e in es:
    if not seen or seen[-1] != e:
        seen.append(e)
print(f"after de-dup-runs: {len(seen)} entries")
print()
# Print last 200 of de-duped
for x in seen[-200:]:
    print(f"  {x}")

# Frequency table
from collections import Counter
print()
print("=== name frequency in last 512 ===")
c = Counter(es)
for n, k in c.most_common():
    print(f"  {k:>5}  {n}")
