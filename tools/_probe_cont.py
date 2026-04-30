"""Probe gControllers[0] before/during/after Start press."""
import socket, json, time, sys

OUT = open(sys.argv[1] if len(sys.argv) > 1 else "build/probe_cont.txt", "w")
def w(s):
    OUT.write(s + "\n"); OUT.flush()

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

call(json.dumps({"cmd":"claim_input"}))
for _ in range(40):
    s = call("status")
    if s.get("frame", 0) >= 120: break
    time.sleep(0.3)
w("booted at frame " + str(s["frame"]))

CONT0 = 0x800A7320
def peek_cont():
    r = call(json.dumps({"cmd":"rdram_peek","addr":CONT0,"n":16}))
    return r.get("hex", "?")

def peek_state():
    return call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4})).get("hex","?")

w("PRE press: gControllers[0] = " + peek_cont())
w("  state pre: " + peek_state())
status = call("status")
w("  override active: " + str(status.get("input_override")) +
  ", buttons override: " + hex(status.get("buttons", 0)))

# Hold Start, peek 100ms intervals
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
status = call("status")
w("  override active (down): " + str(status.get("input_override")) +
  ", buttons override: " + hex(status.get("buttons", 0)))

for i in range(8):
    time.sleep(0.05)
    w("  DURING t={}ms: gC0={} state={}".format(50*(i+1), peek_cont(), peek_state()))

call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(0.5)
w("POST release (500ms): gC0=" + peek_cont())
w("state post: " + peek_state())
time.sleep(2)
w("state @ +2s: " + peek_state())
w("gC0 @ +2s: " + peek_cont())
OUT.close()
