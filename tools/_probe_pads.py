"""Sustained press; peek gControllerPads[0] + gControllers[0] every 50ms."""
import socket, json, time, sys

OUT = open(sys.argv[1] if len(sys.argv) > 1 else "build/probe_pads.txt", "w")
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
w("gPads pre: " + call(json.dumps({"cmd":"rdram_peek","addr":0x800A73E0,"n":32})).get("hex","?"))

call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
for i in range(30):
    time.sleep(0.05)
    pads = call(json.dumps({"cmd":"rdram_peek","addr":0x800A73E0,"n":8})).get("hex","?")
    cont = call(json.dumps({"cmd":"rdram_peek","addr":0x800A7320,"n":12})).get("hex","?")
    state = call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4})).get("hex","?")
    w(f"t={50*i:>4}ms pads={pads} cont={cont} state={state}")

call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(1)
w("after release: state=" + call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4})).get("hex","?"))
OUT.close()
