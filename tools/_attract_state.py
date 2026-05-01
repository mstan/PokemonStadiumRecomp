"""Once attract is stuck, dump comprehensive state."""
import json, socket, sys, time

def call(cmd, t=3.0):
    try:
        s = socket.create_connection(("127.0.0.1", 4371), timeout=t)
    except OSError as e:
        return {"_err": str(e)}
    try:
        s.sendall((json.dumps(cmd)+"\n").encode())
        s.settimeout(t); buf=b""
        while True:
            try: c=s.recv(65536)
            except socket.timeout: break
            if not c: break
            buf += c
            if buf.endswith(b"\n"): break
    finally: s.close()
    try: return json.loads(buf.decode(errors="replace").strip())
    except: return {"_err": "parse"}

print("=== status ===")
print(json.dumps(call({"cmd":"status"}), indent=2))

print()
print("=== trace_recent (last 50) ===")
tr = call({"cmd":"trace_recent","n":50})
if isinstance(tr, dict) and isinstance(tr.get("entries"), list):
    for e in tr["entries"]:
        print(" ", e)

print()
print("=== libultra_recent (last 30) ===")
lr = call({"cmd":"libultra_recent","n":30})
if isinstance(lr, dict) and isinstance(lr.get("events"), list):
    for e in lr["events"]:
        print(" ", e.get("name"), "ms=", e.get("ms"))

print()
print("=== gCurrentGameState (vram 0x80075668), D_8438E79C, D_8438E7AC, D_800AE540.unk_0000 ===")
peeks = [
    ("gCurrentGameState 0x80075668", 0x80075668, 4),
    ("D_8438E79C",                   0x8438E79C, 4),
    ("D_8438E7AC",                   0x8438E7AC, 4),
    ("D_800AE540 hdr",               0x800AE540, 16),
]
for name, addr, n in peeks:
    r = call({"cmd":"rdram_peek","addr":addr,"n":n})
    print(f"  {name}: {r}")
