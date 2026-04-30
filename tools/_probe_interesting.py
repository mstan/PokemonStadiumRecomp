"""Probe interesting-fn counters before and after a Start press."""
import socket, json, time, sys

OUT = open(sys.argv[1] if len(sys.argv) > 1 else "build/probe_interesting.txt", "w")
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
w(f"booted at frame {s.get('frame')}")

def snap():
    r = call("interesting_fns")
    return {f["name"]: f["count"] for f in r.get("fns", [])}

s0 = snap()
w("PRE press counts:")
for k, v in s0.items():
    w(f"  {k}: {v}")

call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
time.sleep(2.0)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(1.0)

s1 = snap()
w(f"POST press (3s elapsed) counts (delta):")
for k, v in s1.items():
    delta = v - s0.get(k, 0)
    w(f"  {k}: {v} (+{delta})")

w(f"final state: " + call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4})).get("hex","?"))
OUT.close()
