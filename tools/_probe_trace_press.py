"""Press Start and observe trace ring for fragment17 / game-thread activity."""
import socket, json, time, sys

OUT = open(sys.argv[1] if len(sys.argv) > 1 else "build/probe_trace.txt", "w")
def w(s): OUT.write(s + "\n"); OUT.flush()

def call(c, t=3):
    s = socket.create_connection(("127.0.0.1", 4371), timeout=t)
    s.sendall((c + "\n").encode())
    buf = b""
    while not buf.endswith(b"\n"):
        x = s.recv(65536)
        if not x: break
        buf += x
    s.close()
    return json.loads(buf.decode().strip())

call(json.dumps({"cmd":"claim_input"}))
for _ in range(40):
    s = call("status")
    if s.get("frame", 0) >= 120: break
    time.sleep(0.3)
w("booted at " + str(s.get("frame")))

# Snapshot trace BEFORE press
r0 = call(json.dumps({"cmd":"trace_recent","n":256}))
e0 = r0.get("entries", [])
w(f"trace pre: write_idx={r0.get('write_idx')} entries={len(e0)}")

# Press
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
time.sleep(1.5)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(0.5)

# Snapshot trace AFTER press
r1 = call(json.dumps({"cmd":"trace_recent","n":256}))
e1 = r1.get("entries", [])
w(f"trace post: write_idx={r1.get('write_idx')} entries={len(e1)}")
w(f"new entries since pre: {r1.get('write_idx') - r0.get('write_idx')}")

# Look for game-thread / fragment17 / Cont_ReadInputs / func_86B activity
from collections import Counter
counts = Counter(e1)
print_terms = [
    ("Cont_ReadInputs", "Cont_ReadInputs"),
    ("Cont_StartReadInputs", "Cont_StartReadInputs"),
    ("fragment17", "fragment17_entry"),
    ("fragment36", "fragment36_entry"),
    ("func_86B", "func_86B"),
    ("func_800290E4", "func_800290E4"),
    ("func_80029310", "func_80029310"),
    ("Game_Thread", "Game_Thread"),
    ("func_8002B330", "func_8002B330"),
    ("Game_DoCopy", "Game_DoCopyProtection"),
    ("func_800290B4", "func_800290B4"),
]
w("counts in last 256 entries:")
for label, prefix in print_terms:
    n = sum(1 for e in e1 if prefix in e)
    w(f"  {label}: {n}")

w("last 20 ents:")
for e in e1[-20:]:
    w(f"  {e}")
OUT.close()
