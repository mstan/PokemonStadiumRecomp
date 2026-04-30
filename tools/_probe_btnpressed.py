"""Peek gPlayer1Controller->buttonPressed (at 0x800A7328) during sustained press,
plus fragment36's static D_82100EC8 (at 0x8015B398, runtime addr)."""
import socket, json, time, sys

OUT = open(sys.argv[1] if len(sys.argv) > 1 else "build/probe_btn.txt", "w")
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

# Phase 1: press once to load fragment36 (advance state 1->2)
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
time.sleep(0.3)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(1.0)
state = call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4})).get("hex","?")
w(f"after first press: state={state}")

# Phase 2: press again, peek buttonPressed many times very quickly
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
w("PRESS down — sampling buttonPressed every 5ms for 200ms")
for i in range(40):
    bp = call(json.dumps({"cmd":"rdram_peek","addr":0x800A7328,"n":2})).get("hex","?")
    bd = call(json.dumps({"cmd":"rdram_peek","addr":0x800A7326,"n":2})).get("hex","?")
    state = call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4})).get("hex","?")
    w(f"  t={5*i:>3}ms: btnDown={bd} btnPressed={bp} state={state}")
    time.sleep(0.005)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(1)
w("after release: state=" + call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4})).get("hex","?"))
OUT.close()
