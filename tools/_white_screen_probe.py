"""Reproduce + probe the white-screen-after-Start state."""
import socket, json, time, sys

OUT = open(sys.argv[1] if len(sys.argv) > 1 else "build/probe_out.txt", "w")
def out(s):
    OUT.write(s + "\n"); OUT.flush(); print(s, flush=True)

def call(cmd, t=3):
    s = socket.create_connection(("127.0.0.1", 4371), timeout=t)
    s.sendall((cmd + "\n").encode())
    buf = b""
    while not buf.endswith(b"\n"):
        c = s.recv(65536)
        if not c: break
        buf += c
    s.close()
    return json.loads(buf.decode().strip())

out("[probe] connecting...")
out(f"[probe] claim: {call(json.dumps({'cmd':'claim_input'}))}")

for i in range(60):
    s = call("status")
    if s.get("frame", 0) >= 120:
        break
    time.sleep(0.3)
out(f"[probe] booted at frame {s['frame']}")

def peek_state():
    return call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":8})).get("hex")

out(f"[probe] state pre-press: {peek_state()}")

call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
time.sleep(0.1)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
out("[probe] pressed Start")

# Sample state every 500ms for 5s
for i in range(10):
    time.sleep(0.5)
    out(f"[probe] state @ {0.5*(i+1):.1f}s post-press: {peek_state()}")

# Trace ring final
r = call(json.dumps({"cmd":"trace_recent","n":80}))
out("[probe] last 80 trace entries:")
for e in r.get("entries", [])[-80:]:
    out(f"  {e}")

OUT.close()
